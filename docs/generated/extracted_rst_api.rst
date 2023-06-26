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

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Properties`):
            *no description available*


    .. py:method:: mitsuba.AdjointIntegrator.sample(self, scene, sensor, block, sample_scale)

        Sample the incident importance and splat the product of importance and
        radiance to the film.

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            The underlying scene

        Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
            A sensor from which rays should be sampled

        Parameter ``sampler``:
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

    .. py:method:: __init__(self)


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

        Parameter ``ptr`` (capsule):
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


    .. py:method:: __init__(self)


    .. py:method:: mitsuba.ArgParser.add(overloaded)


        .. py:method:: add(self, prefix, extra=False)

            Register a new argument with the given list of prefixes

            Parameter ``prefixes`` (List[str]):
                A list of command prefixes (i.e. {"-f", "--fast"})

            Parameter ``extra`` (bool):
                Indicates whether the argument accepts an extra argument value

            Parameter ``prefix`` (str):
                *no description available*

            Returns → :py:obj:`mitsuba.ArgParser.Arg`:
                *no description available*

        .. py:method:: add(self, prefixes, extra=False)

            Register a new argument with the given prefix

            Parameter ``prefix``:
                A single command prefix (i.e. "-f")

            Parameter ``extra`` (bool):
                Indicates whether the argument accepts an extra argument value

            Returns → :py:obj:`mitsuba.ArgParser.Arg`:
                *no description available*

    .. py:method:: mitsuba.ArgParser.executable_name(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.ArgParser.parse(self, arg0)

        Parse the given set of command line arguments

        Parameter ``arg0`` (List[str]):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.AtomicFloat

    Atomic floating point data type

    The class implements an an atomic floating point data type (which is
    not possible with the existing overloads provided by ``std::atomic``).
    It internally casts floating point values to an integer storage format
    and uses atomic integer compare and exchange operations to perform
    changes.

    .. py:method:: __init__(self, arg0)

        Initialize the AtomicFloat with a given floating point value

        Parameter ``arg0`` (float):
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

        Returns → Tuple[:py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Float]:
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

        Returns → Tuple[:py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Float, :py:obj:`mitsuba.BSDFSample3f`, :py:obj:`mitsuba.Color3f`]:
            *no description available*

    .. py:method:: mitsuba.BSDF.flags(overloaded)


        .. py:method:: flags(self, index, active=True)

            Flags for a specific component of this BSDF.

            Parameter ``index`` (int):
                *no description available*

            Parameter ``active`` (drjit.llvm.ad.Bool):
                Mask to specify active lanes.

            Returns → int:
                *no description available*

        .. py:method:: flags(self)

            Flags for all components combined.

            Returns → int:
                *no description available*

    .. py:method:: mitsuba.BSDF.id(self)

        Return a string identifier

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.BSDF.needs_differentials(self)

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
        multiplies by the cosine foreshorening factor with respect to the
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

        Returns → Tuple[:py:obj:`mitsuba.BSDFSample3f`, :py:obj:`mitsuba.Color3f`]:
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


    .. py:method:: __init__(self, mode=<TransportMode., Radiance)

        //! @}

        Parameter ``mode`` (:py:obj:`mitsuba.TransportMode`):
            *no description available*

        Parameter ``Radiance`` (0>):
            *no description available*

    .. py:method:: __init__(self, mode, type_mask, component)

        Parameter ``mode`` (:py:obj:`mitsuba.TransportMode`):
            *no description available*

        Parameter ``type_mask`` (int):
            *no description available*

        Parameter ``component`` (int):
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

    .. py:method:: mitsuba.BSDFContext.reverse(self)

        Reverse the direction of light transport in the record

        This updates the transport mode (radiance to importance and vice
        versa).

        Returns → None:
            *no description available*

.. py:class:: mitsuba.BSDFFlags

    This list of flags is used to classify the different types of lobes
    that are implemented in a BSDF instance.

    They are also useful for picking out individual components, e.g., by
    setting combinations in BSDFContext::type_mask.

    Members:

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

        Any reflection component (scattering into discrete, 1D, or 2D set of
        directions)

    .. py:data:: Transmission

        Any transmission component (scattering into discrete, 1D, or 2D set of
        directions)

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

    .. py:method:: __init__(self, value)

        Parameter ``value`` (int):
            *no description available*


    .. py:method:: mitsuba.BSDFFlags.name
        :property:

.. py:class:: mitsuba.BSDFPtr


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.BSDF`):
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.BSDFPtr`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.BSDF`:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.eq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.BSDFPtr`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

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

        Returns → Tuple[:py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Float]:
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

        Returns → Tuple[:py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Float, :py:obj:`mitsuba.BSDFSample3f`, :py:obj:`mitsuba.Color3f`]:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.flags(self)

        Flags for all components combined.

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.gather_(source, index, mask, permute=False)

        Parameter ``source`` (:py:obj:`mitsuba.BSDFPtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → :py:obj:`mitsuba.BSDFPtr`:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.needs_differentials(self)

        Does the implementation require access to texture-space differentials?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.neq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.BSDFPtr`):
            *no description available*

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

    .. py:method:: mitsuba.BSDFPtr.registry_get_max_()

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.registry_get_ptr_(arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.reinterpret_array_(arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → :py:obj:`mitsuba.BSDFPtr`:
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
        multiplies by the cosine foreshorening factor with respect to the
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

        Returns → Tuple[:py:obj:`mitsuba.BSDFSample3f`, :py:obj:`mitsuba.Color3f`]:
            A pair (bs, value) consisting of

        bs: Sampling record, indicating the sampled direction, PDF values and
        other information. The contents are undefined if sampling failed.

        value: The BSDF value divided by the probability (multiplied by the
        cosine foreshortening factor when a non-delta component is sampled). A
        zero spectrum indicates that sampling failed.

    .. py:method:: mitsuba.BSDFPtr.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (:py:obj:`mitsuba.BSDFPtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.select_(arg0, arg1, arg2)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.BSDFPtr`):
            *no description available*

        Parameter ``arg2`` (:py:obj:`mitsuba.BSDFPtr`):
            *no description available*

        Returns → :py:obj:`mitsuba.BSDFPtr`:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.zero_

        (arg0: int) -> :py:obj:`mitsuba.llvm_ad_rgb.BSDFPtr`

.. py:class:: mitsuba.BSDFSample3f

    Data structure holding the result of BSDF sampling operations.


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, wo)

        Given a surface interaction and an incident/exitant direction pair
        (wi, wo), create a query record to evaluate the BSDF or its sampling
        density.

        By default, all components will be sampled regardless of what measure
        they live on.

        Parameter ``wo`` (:py:obj:`mitsuba.Vector3f`):
            An outgoing direction in local coordinates. This should be a
            normalized direction vector that points *away* from the scattering
            event.

    .. py:method:: __init__(self, bs)

        Copy constructor

        Parameter ``bs`` (:py:obj:`mitsuba.BSDFSample3f`):
            *no description available*

    .. py:method:: mitsuba.BSDFSample3f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.BSDFSample3f`):
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

        Create a bitmap of the specified type and allocate the necessary
        amount of memory

        Parameter ``pixel_format`` (:py:obj:`mitsuba.Bitmap.PixelFormat`):
            Specifies the pixel format (e.g. RGBA or Luminance-only)

        Parameter ``component_format`` (:py:obj:`mitsuba.Struct.Type`):
            Specifies how the per-pixel components are encoded (e.g. unsigned
            8 bit integers or 32-bit floating point values). The component
            format struct_type_v<Float> will be translated to the
            corresponding compile-time precision type (Float32 or Float64).

        Parameter ``size`` (:py:obj:`mitsuba.Vector`):
            Specifies the horizontal and vertical bitmap size in pixels

        Parameter ``channel_count`` (int):
            Channel count of the image. This parameter is only required when
            ``pixel_format`` = PixelFormat::MultiChannel

        Parameter ``channel_names`` (List[str]):
            Channel names of the image. This parameter is optional, and only
            used when ``pixel_format`` = PixelFormat::MultiChannel

        Parameter ``data``:
            External pointer to the image data. If set to ``nullptr``, the
            implementation will allocate memory itself.

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Bitmap`):
            *no description available*

    .. py:method:: __init__(self, path, format=<FileFormat., Auto)

        Parameter ``path`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Parameter ``format`` (:py:obj:`mitsuba.Bitmap.FileFormat`):
            *no description available*

        Parameter ``Auto`` (9>):
            *no description available*

    .. py:method:: __init__(self, stream, format=<FileFormat., Auto)

        Parameter ``stream`` (:py:obj:`mitsuba.Stream`):
            *no description available*

        Parameter ``format`` (:py:obj:`mitsuba.Bitmap.FileFormat`):
            *no description available*

        Parameter ``Auto`` (9>):
            *no description available*

    .. py:method:: __init__(self, array, pixel_format=None, channel_names=[])

        Initialize a Bitmap from any array that implements ``__array_interface__``

        Parameter ``array`` (:py:obj:`mitsuba.PyObjectWrapper`):
            *no description available*

        Parameter ``pixel_format`` (object):
            *no description available*

        Parameter ``channel_names`` (List[str]):
            *no description available*

    .. py:class:: mitsuba.Bitmap.AlphaTransform

        Type of alpha transformation

        Members:

        .. py:data:: Empty

            No transformation (default)

        .. py:data:: Premultiply

            No transformation (default)

        .. py:data:: Unpremultiply

            No transformation (default)

        .. py:method:: __init__(self, value)

            Parameter ``value`` (int):
                *no description available*


    .. py:method:: mitsuba.Bitmap.AlphaTransform.name
        :property:

    .. py:class:: mitsuba.Bitmap.FileFormat

        Supported image file formats

        Members:

        .. py:data:: PNG

            Portable network graphics

            The following is supported:

            * Loading and saving of 8/16-bit per component bitmaps for all pixel
              formats (Y, YA, RGB, RGBA)

            * Loading and saving of 1-bit per component mask bitmaps

            * Loading and saving of string-valued metadata fields

        .. py:data:: OpenEXR

            OpenEXR high dynamic range file format developed by Industrial Light &
            Magic (ILM)

            The following is supported:

            * Loading and saving of Float16 / Float32/ UInt32 bitmaps with all
              supported RGB/Luminance/Alpha combinations

            * Loading and saving of spectral bitmaps

            * Loading and saving of XYZ tristimulus bitmaps

            * Loading and saving of string-valued metadata fields

            The following is *not* supported:

            * Saving of tiled images, tile-based read access

            * Display windows that are different than the data window

            * Loading of spectrum-valued bitmaps

        .. py:data:: RGBE

            RGBE image format by Greg Ward

            The following is supported

            * Loading and saving of Float32 - based RGB bitmaps

        .. py:data:: PFM

            PFM (Portable Float Map) image format

            The following is supported

            * Loading and saving of Float32 - based Luminance or RGB bitmaps

        .. py:data:: PPM

            PPM (Portable Pixel Map) image format

            The following is supported

            * Loading and saving of UInt8 and UInt16 - based RGB bitmaps

        .. py:data:: JPEG

            Joint Photographic Experts Group file format

            The following is supported:

            * Loading and saving of 8 bit per component RGB and luminance bitmaps

        .. py:data:: TGA

            Truevision Advanced Raster Graphics Array file format

            The following is supported:

            * Loading of uncompressed 8-bit RGB/RGBA files

        .. py:data:: BMP

            Windows Bitmap file format

            The following is supported:

            * Loading of uncompressed 8-bit luminance and RGBA bitmaps

        .. py:data:: Unknown

            Unknown file format

        .. py:data:: Auto

            Automatically detect the file format

            Note: this flag only applies when loading a file. In this case, the
            source stream must support the ``seek()`` operation.

        .. py:method:: __init__(self, value)

            Parameter ``value`` (int):
                *no description available*


    .. py:method:: mitsuba.Bitmap.FileFormat.name
        :property:

    .. py:class:: mitsuba.Bitmap.PixelFormat

        This enumeration lists all pixel format types supported by the Bitmap
        class. This both determines the number of channels, and how they
        should be interpreted

        Members:

        .. py:data:: Y

            Single-channel luminance bitmap

        .. py:data:: YA

            Two-channel luminance + alpha bitmap

        .. py:data:: RGB

            RGB bitmap

        .. py:data:: RGBA

            RGB bitmap + alpha channel

        .. py:data:: RGBW

            RGB bitmap + weight (used by ImageBlock)

        .. py:data:: RGBAW

            RGB bitmap + alpha channel + weight (used by ImageBlock)

        .. py:data:: XYZ

            XYZ tristimulus bitmap

        .. py:data:: XYZA

            XYZ tristimulus + alpha channel

        .. py:data:: MultiChannel

            Arbitrary multi-channel bitmap without a fixed interpretation

        .. py:method:: __init__(self, value)

            Parameter ``value`` (int):
                *no description available*


    .. py:method:: mitsuba.Bitmap.PixelFormat.name
        :property:

    .. py:method:: mitsuba.Bitmap.accumulate(overloaded)


        .. py:method:: accumulate(self, bitmap, source_offset)

            Accumulate the contents of another bitmap into the region with the
            specified offset

            Out-of-bounds regions are safely ignored. It is assumed that ``bitmap
            != this``.

            Remark:
                This function throws an exception when the bitmaps use different
                component formats or channels.

            Parameter ``bitmap`` (:py:obj:`mitsuba.Bitmap`):
                *no description available*

            Parameter ``source_offset`` (:py:obj:`mitsuba.Point`):
                *no description available*

        .. py:method:: accumulate(self, bitmap, target_offset)

            Accumulate the contents of another bitmap into the region with the
            specified offset

            This convenience function calls the main ``accumulate()``
            implementation with ``size`` set to ``bitmap->size()`` and
            ``source_offset`` set to zero. Out-of-bounds regions are ignored. It
            is assumed that ``bitmap != this``.

            Remark:
                This function throws an exception when the bitmaps use different
                component formats or channels.

            Parameter ``bitmap`` (:py:obj:`mitsuba.Bitmap`):
                *no description available*

            Parameter ``target_offset`` (:py:obj:`mitsuba.Point`):
                *no description available*

        .. py:method:: accumulate(self, bitmap)

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

    .. py:method:: mitsuba.Bitmap.buffer_size(self)

        Return the bitmap size in bytes (excluding metadata)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bitmap.bytes_per_pixel(self)

        Return the number bytes of storage used per pixel

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bitmap.channel_count(self)

        Return the number of channels used by this bitmap

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bitmap.clear(self)

        Clear the bitmap to zero

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bitmap.component_format(self)

        Return the component format of this bitmap

        Returns → :py:obj:`mitsuba.Struct.Type`:
            *no description available*

    .. py:method:: mitsuba.Bitmap.convert(overloaded)


        .. py:method:: convert(self, pixel_format=None, component_format=None, srgb_gamma=None, alpha_transform=<AlphaTransform., Empty)

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

            Parameter ``pixel_format`` (object):
                *no description available*

            Parameter ``component_format`` (object):
                *no description available*

            Parameter ``srgb_gamma`` (object):
                *no description available*

            Parameter ``alpha_transform`` (:py:obj:`mitsuba.Bitmap.AlphaTransform`):
                *no description available*

            Parameter ``Empty`` (0>):
                *no description available*

            Returns → :py:obj:`mitsuba.Bitmap`:
                *no description available*

        .. py:method:: convert(self, target)

            Parameter ``target`` (:py:obj:`mitsuba.Bitmap`):
                *no description available*

    .. py:method:: mitsuba.Bitmap.detect_file_format(arg0)

        Attempt to detect the bitmap file format in a given stream

        Parameter ``arg0`` (:py:obj:`mitsuba.Stream`):
            *no description available*

        Returns → :py:obj:`mitsuba.Bitmap.FileFormat`:
            *no description available*

    .. py:method:: mitsuba.Bitmap.has_alpha(self)

        Return whether this image has an alpha channel

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Bitmap.height(self)

        Return the bitmap's height in pixels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bitmap.metadata(self)

        Return a Properties object containing the image metadata

        Returns → mitsuba::Properties:
            *no description available*

    .. py:method:: mitsuba.Bitmap.pixel_count(self)

        Return the total number of pixels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bitmap.pixel_format(self)

        Return the pixel format of this bitmap

        Returns → :py:obj:`mitsuba.Bitmap.PixelFormat`:
            *no description available*

    .. py:method:: mitsuba.Bitmap.premultiplied_alpha(self)

        Return whether the bitmap uses premultiplied alpha

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Bitmap.resample(overloaded)


        .. py:method:: resample(self, target, rfilter=None, bc=(<FilterBoundaryCondition., Clamp, Clamp, clamp=(-inf, inf), temp=None)

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

            Parameter ``rfilter`` (:py:obj:`mitsuba.ReconstructionFilter`):
                A separable image reconstruction filter (default: 2-lobe Lanczos
                filter)

            Parameter ``bch``:
                Horizontal and vertical boundary conditions (default: clamp)

            Parameter ``clamp`` (Tuple[float, float]):
                Filtered image pixels will be clamped to the following range.
                Default: -infinity..infinity (i.e. no clamping is used)

            Parameter ``temp`` (:py:obj:`mitsuba.Bitmap`):
                Optional: image for intermediate computations

            Parameter ``bc`` (Tuple[:py:obj:`mitsuba.FilterBoundaryCondition`, :py:obj:`mitsuba.FilterBoundaryCondition`]):
                *no description available*

            Parameter ``Clamp`` (0>, <FilterBoundaryCondition.):
                *no description available*

            Parameter ``Clamp`` (0>)):
                *no description available*

        .. py:method:: resample(self, res=None, bc=(<FilterBoundaryCondition., Clamp, Clamp, clamp=(-inf, inf))

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

            Parameter ``res`` (:py:obj:`mitsuba.Vector`):
                Desired output resolution

            Parameter ``rfilter``:
                A separable image reconstruction filter (default: 2-lobe Lanczos
                filter)

            Parameter ``bch``:
                Horizontal and vertical boundary conditions (default: clamp)

            Parameter ``clamp`` (Tuple[float, float]):
                Filtered image pixels will be clamped to the following range.
                Default: -infinity..infinity (i.e. no clamping is used)

            Parameter ``bc`` (Tuple[:py:obj:`mitsuba.FilterBoundaryCondition`, :py:obj:`mitsuba.FilterBoundaryCondition`]):
                *no description available*

            Parameter ``Clamp`` (0>, <FilterBoundaryCondition.):
                *no description available*

            Parameter ``Clamp`` (0>)):
                *no description available*

            Returns → :py:obj:`mitsuba.Bitmap`:
                *no description available*

    .. py:method:: mitsuba.Bitmap.set_premultiplied_alpha(self, arg0)

        Specify whether the bitmap uses premultiplied alpha

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bitmap.set_srgb_gamma(self, arg0)

        Specify whether the bitmap uses an sRGB gamma encoding

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bitmap.size(self)

        Return the bitmap dimensions in pixels

        Returns → :py:obj:`mitsuba.Vector`:
            *no description available*

    .. py:method:: mitsuba.Bitmap.split(self)

        Split an multi-channel image buffer (e.g. from an OpenEXR image with
        lots of AOVs) into its constituent layers

        Returns → List[Tuple[str, :py:obj:`mitsuba.Bitmap`]]:
            *no description available*

    .. py:method:: mitsuba.Bitmap.srgb_gamma(self)

        Return whether the bitmap uses an sRGB gamma encoding

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Bitmap.struct_(self)

        Return a ``Struct`` instance describing the contents of the bitmap
        (const version)

        Returns → :py:obj:`mitsuba.Struct`:
            *no description available*

    .. py:method:: mitsuba.Bitmap.vflip(self)

        Vertically flip the bitmap

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bitmap.width(self)

        Return the bitmap's width in pixels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bitmap.write(overloaded)


        .. py:method:: write(self, stream, format=<FileFormat., Auto, quality=-1)

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

            Parameter ``Auto`` (9>):
                *no description available*

        .. py:method:: write(self, path, format=<FileFormat., Auto, quality=-1)

            Write an encoded form of the bitmap to a file using the specified file
            format

            Parameter ``path`` (:py:obj:`mitsuba.filesystem.path`):
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

            Parameter ``Auto`` (9>):
                *no description available*

    .. py:method:: mitsuba.Bitmap.write_async(self, path, format=<FileFormat., Auto, quality=-1)

        Equivalent to write(), but executes asynchronously on a different
        thread

        Parameter ``path`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Parameter ``format`` (:py:obj:`mitsuba.Bitmap.FileFormat`):
            *no description available*

        Parameter ``Auto`` (9>):
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

    .. py:method:: mitsuba.BitmapReconstructionFilter.border_size(self)

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

    .. py:method:: mitsuba.BitmapReconstructionFilter.is_box_filter(self)

        Check whether this is a box filter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.BitmapReconstructionFilter.radius(self)

        Return the filter's width

        Returns → float:
            *no description available*

.. py:class:: mitsuba.Bool


    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (bool):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.Bool):
            *no description available*

    .. py:method:: __init__(self)

    .. py:method:: mitsuba.Bool.all_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Bool.and_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.andnot_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.any_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Bool.assign(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bool.compress_(self)

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.Bool.copy_(self)

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.count_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bool.data_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bool.detach_(self)

        Returns → drjit.llvm.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.detach_ref_(self)

        Returns → drjit.llvm.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Bool.eq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.full_

        (arg0: bool, arg1: int) -> drjit.llvm.ad.Bool

    .. py:method:: mitsuba.Bool.gather_(source, index, mask, permute=False)

        Parameter ``source`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.iand_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.init_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bool.ior_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.is_evaluated_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Bool.is_literal_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Bool.ixor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Bool.load_(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.map_(ptr, size, callback=None)

        Parameter ``ptr`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Parameter ``callback`` (Callable[[], None]):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.migrate_(self, arg0)

        Parameter ``arg0`` (AllocType):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.neq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.not_(self)

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.opaque_

        (arg0: bool, arg1: int) -> drjit.llvm.ad.Bool

    .. py:method:: mitsuba.Bool.or_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.resize_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bool.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bool.select_

        (arg0: drjit.llvm.ad.Bool, arg1: drjit.llvm.ad.Bool, arg2: drjit.llvm.ad.Bool) -> drjit.llvm.ad.Bool

    .. py:method:: mitsuba.Bool.set_entry_(self, arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bool.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bool.set_index_ad_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bool.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bool.xor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Bool.zero_

        (arg0: int) -> drjit.llvm.ad.Bool

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


    .. py:method:: __init__(self)

        Create a new invalid bounding box

        Initializes the components of the minimum and maximum position to
        :math:`\infty` and :math:`-\infty`, respectively.

    .. py:method:: __init__(self, p)

        Create a collapsed bounding box from a single point

        Parameter ``p`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

    .. py:method:: __init__(self, min, max)

        Create a bounding box from two positions

        Parameter ``min`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``max`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.BoundingBox2f`):
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.center(self)

        Return the center point

        Returns → :py:obj:`mitsuba.Point2f`:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.clip(self, arg0)

        Clip this bounding box to another bounding box

        Parameter ``arg0`` (:py:obj:`mitsuba.BoundingBox2f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.collapsed(self)

        Check whether this bounding box has collapsed to a point, line, or
        plane

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.contains(overloaded)


        .. py:method:: contains(self, p, strict=False)

            Check whether a point lies *on* or *inside* the bounding box

            Parameter ``p`` (:py:obj:`mitsuba.Point2f`):
                The point to be tested

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

        .. py:method:: contains(self, bbox, strict=False)

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

            Parameter ``bbox`` (:py:obj:`mitsuba.BoundingBox2f`):
                *no description available*

            Parameter ``strict`` (bool):
                *no description available*

            Returns → drjit.llvm.ad.Bool:
                *no description available*

    .. py:method:: mitsuba.BoundingBox2f.corner(self, arg0)

        Return the position of a bounding box corner

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.Point2f`:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.distance(overloaded)


        .. py:method:: distance(self, arg0)

            Calculate the shortest distance between the axis-aligned bounding box
            and the point ``p``.

            Parameter ``arg0`` (:py:obj:`mitsuba.Point2f`):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: distance(self, arg0)

            Calculate the shortest distance between the axis-aligned bounding box
            and ``bbox``.

            Parameter ``arg0`` (:py:obj:`mitsuba.BoundingBox2f`):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.BoundingBox2f.expand(overloaded)


        .. py:method:: expand(self, arg0)

            Expand the bounding box to contain another point

            Parameter ``arg0`` (:py:obj:`mitsuba.Point2f`):
                *no description available*

        .. py:method:: expand(self, arg0)

            Expand the bounding box to contain another bounding box

            Parameter ``arg0`` (:py:obj:`mitsuba.BoundingBox2f`):
                *no description available*

    .. py:method:: mitsuba.BoundingBox2f.extents(self)

        Calculate the bounding box extents

        Returns → :py:obj:`mitsuba.Vector2f`:
            ``max - min``

    .. py:method:: mitsuba.BoundingBox2f.major_axis(self)

        Return the dimension index with the index associated side length

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.merge(arg0, arg1)

        Merge two bounding boxes

        Parameter ``arg0`` (:py:obj:`mitsuba.BoundingBox2f`):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.BoundingBox2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.BoundingBox2f`:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.minor_axis(self)

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

    .. py:method:: mitsuba.BoundingBox2f.reset(self)

        Mark the bounding box as invalid.

        This operation sets the components of the minimum and maximum position
        to :math:`\infty` and :math:`-\infty`, respectively.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.squared_distance(overloaded)


        .. py:method:: squared_distance(self, arg0)

            Calculate the shortest squared distance between the axis-aligned
            bounding box and the point ``p``.

            Parameter ``arg0`` (:py:obj:`mitsuba.Point2f`):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: squared_distance(self, arg0)

            Calculate the shortest squared distance between the axis-aligned
            bounding box and ``bbox``.

            Parameter ``arg0`` (:py:obj:`mitsuba.BoundingBox2f`):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.BoundingBox2f.surface_area(self)

        Calculate the 2-dimensional surface area of a 3D bounding box

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.valid(self)

        Check whether this is a valid bounding box

        A bounding box ``bbox`` is considered to be valid when

        .. code-block:: c

            bbox.min[i] <= bbox.max[i]


        holds for each component ``i``.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.volume(self)

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


    .. py:method:: __init__(self)

        Create a new invalid bounding box

        Initializes the components of the minimum and maximum position to
        :math:`\infty` and :math:`-\infty`, respectively.

    .. py:method:: __init__(self, p)

        Create a collapsed bounding box from a single point

        Parameter ``p`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

    .. py:method:: __init__(self, min, max)

        Create a bounding box from two positions

        Parameter ``min`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``max`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.BoundingBox3f`):
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.bounding_sphere(self)

        Create a bounding sphere, which contains the axis-aligned box

        Returns → :py:obj:`mitsuba.BoundingSphere`:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.center(self)

        Return the center point

        Returns → :py:obj:`mitsuba.Point3f`:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.clip(self, arg0)

        Clip this bounding box to another bounding box

        Parameter ``arg0`` (:py:obj:`mitsuba.BoundingBox3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.collapsed(self)

        Check whether this bounding box has collapsed to a point, line, or
        plane

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.contains(overloaded)


        .. py:method:: contains(self, p, strict=False)

            Check whether a point lies *on* or *inside* the bounding box

            Parameter ``p`` (:py:obj:`mitsuba.Point3f`):
                The point to be tested

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

        .. py:method:: contains(self, bbox, strict=False)

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

            Parameter ``bbox`` (:py:obj:`mitsuba.BoundingBox3f`):
                *no description available*

            Parameter ``strict`` (bool):
                *no description available*

            Returns → drjit.llvm.ad.Bool:
                *no description available*

    .. py:method:: mitsuba.BoundingBox3f.corner(self, arg0)

        Return the position of a bounding box corner

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.Point3f`:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.distance(overloaded)


        .. py:method:: distance(self, arg0)

            Calculate the shortest distance between the axis-aligned bounding box
            and the point ``p``.

            Parameter ``arg0`` (:py:obj:`mitsuba.Point3f`):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: distance(self, arg0)

            Calculate the shortest distance between the axis-aligned bounding box
            and ``bbox``.

            Parameter ``arg0`` (:py:obj:`mitsuba.BoundingBox3f`):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.BoundingBox3f.expand(overloaded)


        .. py:method:: expand(self, arg0)

            Expand the bounding box to contain another point

            Parameter ``arg0`` (:py:obj:`mitsuba.Point3f`):
                *no description available*

        .. py:method:: expand(self, arg0)

            Expand the bounding box to contain another bounding box

            Parameter ``arg0`` (:py:obj:`mitsuba.BoundingBox3f`):
                *no description available*

    .. py:method:: mitsuba.BoundingBox3f.extents(self)

        Calculate the bounding box extents

        Returns → :py:obj:`mitsuba.Vector3f`:
            ``max - min``

    .. py:method:: mitsuba.BoundingBox3f.major_axis(self)

        Return the dimension index with the index associated side length

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.merge(arg0, arg1)

        Merge two bounding boxes

        Parameter ``arg0`` (:py:obj:`mitsuba.BoundingBox3f`):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.BoundingBox3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.BoundingBox3f`:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.minor_axis(self)

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

        Returns → Tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.reset(self)

        Mark the bounding box as invalid.

        This operation sets the components of the minimum and maximum position
        to :math:`\infty` and :math:`-\infty`, respectively.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.squared_distance(overloaded)


        .. py:method:: squared_distance(self, arg0)

            Calculate the shortest squared distance between the axis-aligned
            bounding box and the point ``p``.

            Parameter ``arg0`` (:py:obj:`mitsuba.Point3f`):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: squared_distance(self, arg0)

            Calculate the shortest squared distance between the axis-aligned
            bounding box and ``bbox``.

            Parameter ``arg0`` (:py:obj:`mitsuba.BoundingBox3f`):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.BoundingBox3f.surface_area(self)

        Calculate the 2-dimensional surface area of a 3D bounding box

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.valid(self)

        Check whether this is a valid bounding box

        A bounding box ``bbox`` is considered to be valid when

        .. code-block:: c

            bbox.min[i] <= bbox.max[i]


        holds for each component ``i``.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.volume(self)

        Calculate the n-dimensional volume of the bounding box

        Returns → drjit.llvm.ad.Float:
            *no description available*

.. py:class:: mitsuba.BoundingSphere3f

    Generic n-dimensional bounding sphere data structure


    .. py:method:: __init__(self)

        Construct bounding sphere(s) at the origin having radius zero

    .. py:method:: __init__(self, arg0, arg1)

        Create bounding sphere(s) from given center point(s) with given
        size(s)

        Parameter ``arg0`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.BoundingSphere3f`):
            *no description available*

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

    .. py:method:: mitsuba.BoundingSphere3f.empty(self)

        Return whether this bounding sphere has a radius of zero or less.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BoundingSphere3f.expand(self, arg0)

        Expand the bounding sphere radius to contain another point.

        Parameter ``arg0`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BoundingSphere3f.ray_intersect(self, ray)

        Check if a ray intersects a bounding box

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Returns → Tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.ChainScalarTransform3d

    Base class: :py:obj:`mitsuba.ScalarTransform3d`

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: mitsuba.ChainScalarTransform3d.rotate(self, angle)

        Create a rotation transformation in 2D. The angle is specified in
        degrees

        Parameter ``angle`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform3d`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform3d.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint2d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform3d`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform3d.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint2d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform3d`:
            *no description available*

.. py:class:: mitsuba.ChainScalarTransform3f

    Base class: :py:obj:`mitsuba.ScalarTransform3f`

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: mitsuba.ChainScalarTransform3f.rotate(self, angle)

        Create a rotation transformation in 2D. The angle is specified in
        degrees

        Parameter ``angle`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform3f`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform3f.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform3f`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform3f.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform3f`:
            *no description available*

.. py:class:: mitsuba.ChainScalarTransform4d

    Base class: :py:obj:`mitsuba.ScalarTransform4d`

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: mitsuba.ChainScalarTransform4d.from_frame(self, frame)

        Creates a transformation that converts from 'frame' to the standard
        basis

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4d.look_at(self, origin, target, up)

        Create a look-at camera transformation

        Parameter ``origin`` (:py:obj:`mitsuba.ScalarPoint3d`):
            Camera position

        Parameter ``target`` (:py:obj:`mitsuba.ScalarPoint3d`):
            Target vector

        Parameter ``up`` (:py:obj:`mitsuba.ScalarPoint3d`):
            Up vector

        Returns → :py:obj:`mitsuba.ChainScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4d.orthographic(self, near, far)

        Create an orthographic transformation, which maps Z to [0,1] and
        leaves the X and Y coordinates untouched.

        Parameter ``near`` (float):
            Near clipping plane

        Parameter ``far`` (float):
            Far clipping plane

        Returns → :py:obj:`mitsuba.ChainScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4d.perspective(self, fov, near, far)

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

        Returns → :py:obj:`mitsuba.ChainScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4d.rotate(self, axis, angle)

        Create a rotation transformation around an arbitrary axis in 3D. The
        angle is specified in degrees

        Parameter ``axis`` (:py:obj:`mitsuba.ScalarPoint3d`):
            *no description available*

        Parameter ``angle`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4d.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint3d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4d.to_frame(self, frame)

        Creates a transformation that converts from the standard basis to
        'frame'

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4d.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint3d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform4d`:
            *no description available*

.. py:class:: mitsuba.ChainScalarTransform4f

    Base class: :py:obj:`mitsuba.ScalarTransform4f`

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: mitsuba.ChainScalarTransform4f.from_frame(self, frame)

        Creates a transformation that converts from 'frame' to the standard
        basis

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4f.look_at(self, origin, target, up)

        Create a look-at camera transformation

        Parameter ``origin`` (:py:obj:`mitsuba.ScalarPoint3f`):
            Camera position

        Parameter ``target`` (:py:obj:`mitsuba.ScalarPoint3f`):
            Target vector

        Parameter ``up`` (:py:obj:`mitsuba.ScalarPoint3f`):
            Up vector

        Returns → :py:obj:`mitsuba.ChainScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4f.orthographic(self, near, far)

        Create an orthographic transformation, which maps Z to [0,1] and
        leaves the X and Y coordinates untouched.

        Parameter ``near`` (float):
            Near clipping plane

        Parameter ``far`` (float):
            Far clipping plane

        Returns → :py:obj:`mitsuba.ChainScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4f.perspective(self, fov, near, far)

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

        Returns → :py:obj:`mitsuba.ChainScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4f.rotate(self, axis, angle)

        Create a rotation transformation around an arbitrary axis in 3D. The
        angle is specified in degrees

        Parameter ``axis`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

        Parameter ``angle`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4f.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4f.to_frame(self, frame)

        Creates a transformation that converts from the standard basis to
        'frame'

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainScalarTransform4f.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainScalarTransform4f`:
            *no description available*

.. py:class:: mitsuba.ChainTransform3d

    Base class: :py:obj:`mitsuba.Transform3d`

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: mitsuba.ChainTransform3d.rotate(self, angle)

        Create a rotation transformation in 2D. The angle is specified in
        degrees

        Parameter ``angle`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform3d`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform3d.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point2d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform3d`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform3d.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point2d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform3d`:
            *no description available*

.. py:class:: mitsuba.ChainTransform3f

    Base class: :py:obj:`mitsuba.Transform3f`

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: mitsuba.ChainTransform3f.rotate(self, angle)

        Create a rotation transformation in 2D. The angle is specified in
        degrees

        Parameter ``angle`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform3f`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform3f.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform3f`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform3f.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform3f`:
            *no description available*

.. py:class:: mitsuba.ChainTransform4d

    Base class: :py:obj:`mitsuba.Transform4d`

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: mitsuba.ChainTransform4d.from_frame(self, frame)

        Creates a transformation that converts from 'frame' to the standard
        basis

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4d.look_at(self, origin, target, up)

        Create a look-at camera transformation

        Parameter ``origin`` (:py:obj:`mitsuba.Point3d`):
            Camera position

        Parameter ``target`` (:py:obj:`mitsuba.Point3d`):
            Target vector

        Parameter ``up`` (:py:obj:`mitsuba.Point3d`):
            Up vector

        Returns → :py:obj:`mitsuba.ChainTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4d.orthographic(self, near, far)

        Create an orthographic transformation, which maps Z to [0,1] and
        leaves the X and Y coordinates untouched.

        Parameter ``near`` (drjit.llvm.ad.Float64):
            Near clipping plane

        Parameter ``far`` (drjit.llvm.ad.Float64):
            Far clipping plane

        Returns → :py:obj:`mitsuba.ChainTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4d.perspective(self, fov, near, far)

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

        Returns → :py:obj:`mitsuba.ChainTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4d.rotate(self, axis, angle)

        Create a rotation transformation around an arbitrary axis in 3D. The
        angle is specified in degrees

        Parameter ``axis`` (:py:obj:`mitsuba.Point3d`):
            *no description available*

        Parameter ``angle`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4d.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point3d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4d.to_frame(self, frame)

        Creates a transformation that converts from the standard basis to
        'frame'

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4d.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point3d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform4d`:
            *no description available*

.. py:class:: mitsuba.ChainTransform4f

    Base class: :py:obj:`mitsuba.Transform4f`

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: mitsuba.ChainTransform4f.from_frame(self, frame)

        Creates a transformation that converts from 'frame' to the standard
        basis

        Parameter ``frame`` (:py:obj:`mitsuba.Frame3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4f.look_at(self, origin, target, up)

        Create a look-at camera transformation

        Parameter ``origin`` (:py:obj:`mitsuba.Point3f`):
            Camera position

        Parameter ``target`` (:py:obj:`mitsuba.Point3f`):
            Target vector

        Parameter ``up`` (:py:obj:`mitsuba.Point3f`):
            Up vector

        Returns → :py:obj:`mitsuba.ChainTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4f.orthographic(self, near, far)

        Create an orthographic transformation, which maps Z to [0,1] and
        leaves the X and Y coordinates untouched.

        Parameter ``near`` (drjit.llvm.ad.Float):
            Near clipping plane

        Parameter ``far`` (drjit.llvm.ad.Float):
            Far clipping plane

        Returns → :py:obj:`mitsuba.ChainTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4f.perspective(self, fov, near, far)

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

        Returns → :py:obj:`mitsuba.ChainTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4f.rotate(self, axis, angle)

        Create a rotation transformation around an arbitrary axis in 3D. The
        angle is specified in degrees

        Parameter ``axis`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``angle`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4f.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4f.to_frame(self, frame)

        Creates a transformation that converts from the standard basis to
        'frame'

        Parameter ``frame`` (:py:obj:`mitsuba.Frame3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ChainTransform4f.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ChainTransform4f`:
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

    .. py:method:: mitsuba.Class.alias(self)

        Return the scene description-specific alias, if applicable

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Class.name(self)

        Return the name of the class

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Class.parent(self)

        Return the Class object associated with the parent class of nullptr if
        it does not have one.

        Returns → :py:obj:`mitsuba.Class`:
            *no description available*

    .. py:method:: mitsuba.Class.variant(self)

        Return the variant of the class

        Returns → str:
            *no description available*

.. py:class:: mitsuba.Color0d

.. py:class:: mitsuba.Color0f

.. py:class:: mitsuba.Color1d

.. py:class:: mitsuba.Color1f

.. py:class:: mitsuba.Color3d

.. py:class:: mitsuba.Color3f

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


    .. py:method:: __init__(self)

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

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.ContinuousDistribution`):
            *no description available*

    .. py:method:: __init__(self, range, pdf)

        Initialize from a given density function on the interval ``range``

        Parameter ``range`` (:py:obj:`mitsuba.ScalarVector2f`):
            *no description available*

        Parameter ``pdf`` (drjit.llvm.ad.Float):
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.cdf(self)

        Return the unnormalized discrete cumulative distribution function over
        intervals

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.empty(self)

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

    .. py:method:: mitsuba.ContinuousDistribution.integral(self)

        Return the original integral of PDF entries before normalization

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.interval_resolution(self)

        Return the minimum resolution of the discretization

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.max(self)

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.normalization(self)

        Return the normalization factor (i.e. the inverse of sum())

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.pdf(self)

        Return the unnormalized discretized probability density function

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.range(self)

        Return the range of the distribution

        Returns → :py:obj:`mitsuba.ScalarVector2f`:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.sample(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``value`` (drjit.llvm.ad.Float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The sampled position.

    .. py:method:: mitsuba.ContinuousDistribution.sample_pdf(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``value`` (drjit.llvm.ad.Float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            A tuple consisting of

        1. the sampled position. 2. the normalized probability density of the
        sample.

    .. py:method:: mitsuba.ContinuousDistribution.size(self)

        Return the number of discretizations

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.update(self)

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

    .. py:method:: __init__(self)


    .. py:method:: mitsuba.DefaultFormatter.has_class(self)

        See also:
            set_has_class

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.has_date(self)

        See also:
            set_has_date

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.has_log_level(self)

        See also:
            set_has_log_level

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.has_thread(self)

        See also:
            set_has_thread

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.set_has_class(self, arg0)

        Should class information be included? The default is yes.

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.set_has_date(self, arg0)

        Should date information be included? The default is yes.

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.set_has_log_level(self, arg0)

        Should log level information be included? The default is yes.

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.set_has_thread(self, arg0)

        Should thread information be included? The default is yes.

        Parameter ``arg0`` (bool):
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


    .. py:method:: __init__(self)

        Construct an uninitialized direct sample

    .. py:method:: __init__(self, other)

        Construct from a position sample

        Parameter ``other`` (:py:obj:`mitsuba.PositionSample3f`):
            *no description available*

    .. py:method:: __init__(self, other)

        Copy constructor

        Parameter ``other`` (:py:obj:`mitsuba.DirectionSample3f`):
            *no description available*

    .. py:method:: __init__(self, p, n, uv, time, pdf, delta, d, dist, emitter)

        Element-by-element constructor

        Parameter ``p`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``n`` (:py:obj:`mitsuba.Normal3f`):
            *no description available*

        Parameter ``uv`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``time`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``pdf`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``delta`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``d`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Parameter ``dist`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``emitter`` (:py:obj:`mitsuba.EmitterPtr`):
            *no description available*

    .. py:method:: __init__(self, scene, si, ref)

        Create a position sampling record from a surface intersection

        This is useful to determine the hypothetical sampling density on a
        surface after hitting it using standard ray tracing. This happens for
        instance in path tracing with multiple importance sampling.

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            *no description available*

        Parameter ``ref`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

    .. py:method:: mitsuba.DirectionSample3f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.DirectionSample3f`):
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

.. py:class:: mitsuba.DiscreteDistribution

    Discrete 1D probability distribution

    This data structure represents a discrete 1D probability distribution
    and provides various routines for transforming uniformly distributed
    samples so that they follow the stored distribution. Note that
    unnormalized probability mass functions (PMFs) will automatically be
    normalized during initialization. The associated scale factor can be
    retrieved using the function normalization().


    .. py:method:: __init__(self)

        Discrete 1D probability distribution

        This data structure represents a discrete 1D probability distribution
        and provides various routines for transforming uniformly distributed
        samples so that they follow the stored distribution. Note that
        unnormalized probability mass functions (PMFs) will automatically be
        normalized during initialization. The associated scale factor can be
        retrieved using the function normalization().

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.DiscreteDistribution`):
            *no description available*

    .. py:method:: __init__(self, pmf)

        Initialize from a given probability mass function

        Parameter ``pmf`` (drjit.llvm.ad.Float):
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.cdf(self)

        Return the unnormalized cumulative distribution function

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.empty(self)

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

    .. py:method:: mitsuba.DiscreteDistribution.normalization(self)

        Return the normalization factor (i.e. the inverse of sum())

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.pmf(self)

        Return the unnormalized probability mass function

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.sample(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``value`` (drjit.llvm.ad.Float):
            A uniformly distributed sample on the interval [0, 1].

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

        Returns → Tuple[drjit.llvm.ad.UInt, drjit.llvm.ad.Float]:
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

        Returns → Tuple[drjit.llvm.ad.UInt, drjit.llvm.ad.Float]:
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

        Returns → Tuple[drjit.llvm.ad.UInt, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            A tuple consisting of

        1. the discrete index associated with the sample 2. the re-scaled
        sample value 3. the normalized probability value of the sample

    .. py:method:: mitsuba.DiscreteDistribution.size(self)

        Return the number of entries

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.sum(self)

        Return the original sum of PMF entries before normalization

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.update(self)

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

        Returns → Tuple[:py:obj:`mitsuba.Point2u`, drjit.llvm.ad.Float, :py:obj:`mitsuba.Point2f`]:
            *no description available*

.. py:class:: mitsuba.DummyStream

    Base class: :py:obj:`mitsuba.Stream`

    Stream implementation that never writes to disk, but keeps track of
    the size of the content being written. It can be used, for example, to
    measure the precise amount of memory needed to store serialized
    content.

    .. py:method:: __init__(self)


.. py:class:: mitsuba.Emitter

    Base class: :py:obj:`mitsuba.Endpoint`

    .. py:method:: mitsuba.Emitter.flags(self, active=True)

        Flags for all components combined.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Emitter.is_environment(self)

        Is this an environment map light emitter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Emitter.sampling_weight(self)

        The emitter's sampling weight.

        Returns → float:
            *no description available*

.. py:class:: mitsuba.EmitterFlags

    This list of flags is used to classify the different types of
    emitters.

    Members:

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

    .. py:method:: __init__(self, value)

        Parameter ``value`` (int):
            *no description available*


    .. py:method:: mitsuba.EmitterFlags.name
        :property:

.. py:class:: mitsuba.EmitterPtr


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Emitter`):
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.EmitterPtr`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.Emitter`:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.eq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.EmitterPtr`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.eval(self, si, active=True)

        Given a ray-surface intersection, return the emitted radiance or
        importance traveling along the reverse direction

        This function is e.g. used when an area light source has been hit by a
        ray in a path tracing-style integrator, and it subsequently needs to
        be queried for the emitted radiance along the negative ray direction.
        The default implementation throws an exception, which states that the
        method is not implemented.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction`):
            An intersect record that specifies both the query position and
            direction (using the ``si.wi`` field)

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The emitted radiance or importance

    .. py:method:: mitsuba.EmitterPtr.eval_direction(self, it, active=True)

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
        without also differentiating the sampling technique. Alternatively (or
        additionally), it may be necessary to apply a spherical
        reparameterization to ``ds.d`` to handle visibility-induced
        discontinuities during differentiation. Both steps require re-
        evaluating the contribution of the emitter while tracking derivative
        information through the calculation.

        In contrast to pdf_direction(), evaluating this function can yield a
        nonzero result in the case of emission profiles containing a Dirac
        delta term (e.g. point or directional lights).

        Parameter ``ref``:
            A 3D reference location within the scene, which may influence the
            sampling process.

        Parameter ``ds``:
            A direction sampling record, which specifies the query location.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The incident direct radiance/importance associated with the
            sample.

    .. py:method:: mitsuba.EmitterPtr.flags(self)

        Flags for all components combined.

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.gather_(source, index, mask, permute=False)

        Parameter ``source`` (:py:obj:`mitsuba.EmitterPtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → :py:obj:`mitsuba.EmitterPtr`:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.is_environment(self)

        Is this an environment map light emitter?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.neq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.EmitterPtr`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.pdf_direction(self, it, active=True)

        Evaluate the probability density of the *direct* sampling method
        implemented by the sample_direction() method.

        The returned probability will always be zero when the
        emission/sensitivity profile contains a Dirac delta term (e.g. point
        or directional emitters/sensors).

        Parameter ``ds``:
            A direct sampling record, which specifies the query location.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction`):
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

        Parameter ``ps`` (:py:obj:`mitsuba.PositionSample`):
            The sampled position record.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The corresponding sampling density.

    .. py:method:: mitsuba.EmitterPtr.registry_get_max_()

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.registry_get_ptr_(arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.reinterpret_array_(arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → :py:obj:`mitsuba.EmitterPtr`:
            *no description available*

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

        Parameter ``it`` (:py:obj:`mitsuba.Interaction`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.DirectionSample`, :py:obj:`mitsuba.Color3f`]:
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

        Returns → Tuple[:py:obj:`mitsuba.PositionSample`, drjit.llvm.ad.Float]:
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

        Returns → Tuple[:py:obj:`mitsuba.Ray3f`, :py:obj:`mitsuba.Color3f`]:
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

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction`):
            In the case of a spatially-varying spectral sensitivity/emission
            profile, this parameter conditions sampling on a specific spatial
            position. The ``si.uv`` field must be specified in this case.

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A 1D uniformly distributed random variate

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Color0f`, :py:obj:`mitsuba.Color3f`]:
            The set of sampled wavelengths and (potentially spectrally
            varying) importance weights. The latter account for the difference
            between the profile and the actual used sampling density function.
            In the case of emitters, the weight will include the emitted
            radiance.

    .. py:method:: mitsuba.EmitterPtr.sampling_weight(self)

        The emitter's sampling weight.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (:py:obj:`mitsuba.EmitterPtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.select_(arg0, arg1, arg2)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.EmitterPtr`):
            *no description available*

        Parameter ``arg2`` (:py:obj:`mitsuba.EmitterPtr`):
            *no description available*

        Returns → :py:obj:`mitsuba.EmitterPtr`:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.shape(self)

        Return the shape, to which the emitter is currently attached

        Returns → :py:obj:`mitsuba.ShapePtr`:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.zero_

        (arg0: int) -> :py:obj:`mitsuba.llvm_ad_rgb.EmitterPtr`

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

    .. py:method:: mitsuba.Endpoint.bbox(self)

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

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction`):
            An intersect record that specifies both the query position and
            direction (using the ``si.wi`` field)

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The emitted radiance or importance

    .. py:method:: mitsuba.Endpoint.eval_direction(self, it, active=True)

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
        without also differentiating the sampling technique. Alternatively (or
        additionally), it may be necessary to apply a spherical
        reparameterization to ``ds.d`` to handle visibility-induced
        discontinuities during differentiation. Both steps require re-
        evaluating the contribution of the emitter while tracking derivative
        information through the calculation.

        In contrast to pdf_direction(), evaluating this function can yield a
        nonzero result in the case of emission profiles containing a Dirac
        delta term (e.g. point or directional lights).

        Parameter ``ref``:
            A 3D reference location within the scene, which may influence the
            sampling process.

        Parameter ``ds``:
            A direction sampling record, which specifies the query location.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The incident direct radiance/importance associated with the
            sample.

    .. py:method:: mitsuba.Endpoint.medium(self)

        Return a pointer to the medium that surrounds the emitter

        Returns → :py:obj:`mitsuba.Medium`:
            *no description available*

    .. py:method:: mitsuba.Endpoint.needs_sample_2(self)

        Does the method sample_ray() require a uniformly distributed 2D sample
        for the ``sample2`` parameter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Endpoint.needs_sample_3(self)

        Does the method sample_ray() require a uniformly distributed 2D sample
        for the ``sample3`` parameter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Endpoint.pdf_direction(self, it, active=True)

        Evaluate the probability density of the *direct* sampling method
        implemented by the sample_direction() method.

        The returned probability will always be zero when the
        emission/sensitivity profile contains a Dirac delta term (e.g. point
        or directional emitters/sensors).

        Parameter ``ds``:
            A direct sampling record, which specifies the query location.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction`):
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

        Parameter ``ps`` (:py:obj:`mitsuba.PositionSample`):
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

        Parameter ``it`` (:py:obj:`mitsuba.Interaction`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.DirectionSample`, :py:obj:`mitsuba.Color3f`]:
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

        Returns → Tuple[:py:obj:`mitsuba.PositionSample`, drjit.llvm.ad.Float]:
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

        Returns → Tuple[:py:obj:`mitsuba.Ray3f`, :py:obj:`mitsuba.Color3f`]:
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

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction`):
            In the case of a spatially-varying spectral sensitivity/emission
            profile, this parameter conditions sampling on a specific spatial
            position. The ``si.uv`` field must be specified in this case.

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A 1D uniformly distributed random variate

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Color0f`, :py:obj:`mitsuba.Color3f`]:
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

    .. py:method:: mitsuba.Endpoint.shape(self)

        Return the shape, to which the emitter is currently attached

        Returns → :py:obj:`mitsuba.Shape`:
            *no description available*

    .. py:method:: mitsuba.Endpoint.world_transform(self)

        Return the local space to world space transformation

        Returns → :py:obj:`mitsuba.Transform4f`:
            *no description available*

.. py:class:: mitsuba.FileResolver

    Base class: :py:obj:`mitsuba.Object`

    Simple class for resolving paths on Linux/Windows/Mac OS

    This convenience class looks for a file or directory given its name
    and a set of search paths. The implementation walks through the search
    paths in order and stops once the file is found.


    .. py:method:: __init__(self)

        Initialize a new file resolver with the current working directory

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.FileResolver`):
            *no description available*

    .. py:method:: mitsuba.FileResolver.append(self, arg0)

        Append an entry to the end of the list of search paths

        Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.FileResolver.clear(self)

        Clear the list of search paths

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.FileResolver.prepend(self, arg0)

        Prepend an entry at the beginning of the list of search paths

        Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.FileResolver.resolve(self, arg0)

        Walk through the list of search paths and try to resolve the input
        path

        Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Returns → :py:obj:`mitsuba.filesystem.path`:
            *no description available*

.. py:class:: mitsuba.FileStream

    Base class: :py:obj:`mitsuba.Stream`

    Simple Stream implementation backed-up by a file.

    The underlying file abstraction is ``std::fstream``, and so most
    operations can be expected to behave similarly.

    .. py:method:: __init__(self, p, mode=<EMode., ERead)

        Constructs a new FileStream by opening the file pointed by ``p``.

        The file is opened in read-only or read/write mode as specified by
        ``mode``.

        Throws if trying to open a non-existing file in with write disabled.
        Throws an exception if the file cannot be opened / created.

        Parameter ``p`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Parameter ``mode`` (:py:obj:`mitsuba.FileStream.EMode`):
            *no description available*

        Parameter ``ERead`` (0>):
            *no description available*


    .. py:class:: mitsuba.FileStream.EMode

        Members:

        .. py:data:: ERead

            Opens a file in (binary) read-only mode

        .. py:data:: EReadWrite

            Opens (but never creates) a file in (binary) read-write mode

        .. py:data:: ETruncReadWrite

            Opens (and truncates) a file in (binary) read-write mode

        .. py:method:: __init__(self, value)

            Parameter ``value`` (int):
                *no description available*


    .. py:method:: mitsuba.FileStream.EMode.name
        :property:

    .. py:method:: mitsuba.FileStream.path(self)

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


    .. py:method:: mitsuba.Film.bitmap(self, raw=False)

        Return a bitmap object storing the developed contents of the film

        Parameter ``raw`` (bool):
            *no description available*

        Returns → :py:obj:`mitsuba.Bitmap`:
            *no description available*

    .. py:method:: mitsuba.Film.clear(self)

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

    .. py:method:: mitsuba.Film.crop_offset(self)

        Return the offset of the crop window

        Returns → :py:obj:`mitsuba.ScalarPoint2u`:
            *no description available*

    .. py:method:: mitsuba.Film.crop_size(self)

        Return the size of the crop window

        Returns → :py:obj:`mitsuba.ScalarVector2u`:
            *no description available*

    .. py:method:: mitsuba.Film.develop(self, raw=False)

        Return a image buffer object storing the developed image

        Parameter ``raw`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.Film.flags(self)

        Flags for all properties combined.

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Film.prepare(self, aovs)

        Configure the film for rendering a specified set of extra channels
        (AOVS). Returns the total number of channels that the film will store

        Parameter ``aovs`` (List[str]):
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

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Film.put_block(self, block)

        Merge an image block into the film. This methods should be thread-
        safe.

        Parameter ``block`` (:py:obj:`mitsuba.ImageBlock`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Film.rfilter(self)

        Return the image reconstruction filter (const version)

        Returns → :py:obj:`mitsuba.ReconstructionFilter`:
            *no description available*

    .. py:method:: mitsuba.Film.sample_border(self)

        Should regions slightly outside the image plane be sampled to improve
        the quality of the reconstruction at the edges? This only makes sense
        when reconstruction filters other than the box filter are used.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Film.schedule_storage(self)

        dr::schedule() variables that represent the internal film storage

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Film.sensor_response_function(self)

        Returns the specific Sensor Response Function (SRF) used by the film

        Returns → :py:obj:`mitsuba.Texture`:
            *no description available*

    .. py:method:: mitsuba.Film.size(self)

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

    Members:

    .. py:data:: Empty

        No flags set (default value)

    .. py:data:: Alpha

        The film stores an alpha channel

    .. py:data:: Spectral

        The film stores a spectral representation of the image

    .. py:data:: Special

        The film provides a customized prepare_sample() routine that
        implements a special treatment of the samples before storing them in
        the Image Block.

    .. py:method:: __init__(self, value)

        Parameter ``value`` (int):
            *no description available*


    .. py:method:: mitsuba.FilmFlags.name
        :property:

.. py:class:: mitsuba.FilterBoundaryCondition

    When resampling data to a different resolution using
    Resampler::resample(), this enumeration specifies how lookups
    *outside* of the input domain are handled.

    See also:
        Resampler

    Members:

    .. py:data:: Clamp

        Clamp to the outermost sample position (default)

    .. py:data:: Repeat

        Assume that the input repeats in a periodic fashion

    .. py:data:: Mirror

        Assume that the input is mirrored along the boundary

    .. py:data:: Zero

        Assume that the input function is zero outside of the defined domain

    .. py:data:: One

        Assume that the input function is equal to one outside of the defined
        domain

    .. py:method:: __init__(self, value)

        Parameter ``value`` (int):
            *no description available*


    .. py:method:: mitsuba.FilterBoundaryCondition.name
        :property:

.. py:class:: mitsuba.Float


    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: drjit.llvm.ad.Float, *args) -> None

        Parameter ``arg0`` (drjit.llvm.Float):
            *no description available*

    .. py:method:: mitsuba.Float.abs_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.accum_grad_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.Float):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.acos_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.acosh_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.ad_add_edge_(src_index, dst_index, cb=None)

        Parameter ``src_index`` (int):
            *no description available*

        Parameter ``dst_index`` (int):
            *no description available*

        Parameter ``cb`` (handle):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.ad_dec_ref_(self)

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.ad_dequeue_implicit_(self)

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.ad_enqueue_implicit_(self)

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.ad_extract_implicit_(self)

        Returns → List[int]:
            *no description available*

    .. py:method:: mitsuba.Float.ad_implicit_()

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Float.ad_inc_ref_(self)

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.add_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.and_(overloaded)


        .. py:method:: and_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: and_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.Float.andnot_(overloaded)


        .. py:method:: andnot_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: andnot_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.Float.arange_

        (arg0: int, arg1: int, arg2: int) -> drjit.llvm.ad.Float

    .. py:method:: mitsuba.Float.asin_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.asinh_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.assign(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.atan2_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.atan_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.atanh_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.block_sum_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.cbrt_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.ceil_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.copy_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.cos_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.cosh_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.cot_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.create_(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.csc_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.data_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Float.detach_(self)

        Returns → drjit.llvm.Float:
            *no description available*

    .. py:method:: mitsuba.Float.detach_ref_(self)

        Returns → drjit.llvm.Float:
            *no description available*

    .. py:method:: mitsuba.Float.dot_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.enqueue_(self, arg0)

        Parameter ``arg0`` (drjit.ADMode):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.Float.eq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Float.erf_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.erfinv_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.exp2_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.exp_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.floor_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.fma_(self, arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.full_

        (arg0: float, arg1: int) -> drjit.llvm.ad.Float

    .. py:method:: mitsuba.Float.gather_(source, index, mask, permute=False)

        Parameter ``source`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.ge_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Float.grad_(self)

        Returns → drjit.llvm.Float:
            *no description available*

    .. py:method:: mitsuba.Float.grad_enabled_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Float.gt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Float.iadd_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.iand_(overloaded)


        .. py:method:: iand_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: iand_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.Float.imod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.imul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.init_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.ior_(overloaded)


        .. py:method:: ior_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: ior_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.Float.is_evaluated_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Float.is_literal_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Float.isub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.itruediv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.ixor_(overloaded)


        .. py:method:: ixor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: ixor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.Float.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Float.le_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Float.lgamma_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.linspace_

        (arg0: float, arg1: float, arg2: int, arg3: bool) -> drjit.llvm.ad.Float

    .. py:method:: mitsuba.Float.load_(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.log2_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.log_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.lt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Float.map_(ptr, size, callback=None)

        Parameter ``ptr`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Parameter ``callback`` (Callable[[], None]):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.max_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.maximum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.migrate_(self, arg0)

        Parameter ``arg0`` (AllocType):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.min_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.minimum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.mod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.mul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.neg_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.neq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Float.not_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.opaque_

        (arg0: float, arg1: int) -> drjit.llvm.ad.Float

    .. py:method:: mitsuba.Float.or_(overloaded)


        .. py:method:: or_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: or_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.Float.power_(overloaded)


        .. py:method:: power_(self, arg0)

            Parameter ``arg0`` (float):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: power_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.Float.prod_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.rcp_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.reinterpret_array_(overloaded)


        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.Float.resize_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.round_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.rsqrt_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.scatter_reduce_(self, op, target, index, mask)

        Parameter ``op`` (ReduceOp):
            *no description available*

        Parameter ``target`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.scope_enter_(arg0, arg1)

        Parameter ``arg0`` (drjit.detail.ADScope):
            *no description available*

        Parameter ``arg1`` (List[int]):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.scope_leave_(arg0)

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.sec_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.select_

        (arg0: drjit.llvm.ad.Bool, arg1: drjit.llvm.ad.Float, arg2: drjit.llvm.ad.Float) -> drjit.llvm.ad.Float

    .. py:method:: mitsuba.Float.set_entry_(self, arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (float):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.set_grad_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.Float):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.set_grad_enabled_(self, arg0)

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.set_index_ad_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.sin_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.sincos_(self)

        Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Float.sincosh_(self)

        Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Float.sinh_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.sqrt_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.sub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.sum_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.tan_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.tanh_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.tgamma_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.traverse_(arg0, arg1)

        Parameter ``arg0`` (drjit.ADMode):
            *no description available*

        Parameter ``arg1`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float.truediv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.trunc_(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Float.xor_(overloaded)


        .. py:method:: xor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: xor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.Float.zero_

        (arg0: int) -> drjit.llvm.ad.Float

.. py:class:: mitsuba.Float64


    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: drjit.llvm.ad.Float64, *args) -> None

        Parameter ``arg0`` (drjit.llvm.Float64):
            *no description available*

    .. py:method:: mitsuba.Float64.abs_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.accum_grad_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.Float64):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.acos_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.acosh_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.ad_add_edge_(src_index, dst_index, cb=None)

        Parameter ``src_index`` (int):
            *no description available*

        Parameter ``dst_index`` (int):
            *no description available*

        Parameter ``cb`` (handle):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.ad_dec_ref_(self)

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.ad_dequeue_implicit_(self)

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.ad_enqueue_implicit_(self)

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.ad_extract_implicit_(self)

        Returns → List[int]:
            *no description available*

    .. py:method:: mitsuba.Float64.ad_implicit_()

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Float64.ad_inc_ref_(self)

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.add_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.and_(overloaded)


        .. py:method:: and_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float64):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

        .. py:method:: and_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

    .. py:method:: mitsuba.Float64.andnot_(overloaded)


        .. py:method:: andnot_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float64):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

        .. py:method:: andnot_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

    .. py:method:: mitsuba.Float64.arange_

        (arg0: int, arg1: int, arg2: int) -> drjit.llvm.ad.Float64

    .. py:method:: mitsuba.Float64.asin_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.asinh_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.assign(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.atan2_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.atan_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.atanh_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.block_sum_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.cbrt_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.ceil_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.copy_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.cos_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.cosh_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.cot_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.create_(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.csc_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.data_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Float64.detach_(self)

        Returns → drjit.llvm.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.detach_ref_(self)

        Returns → drjit.llvm.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.dot_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.enqueue_(self, arg0)

        Parameter ``arg0`` (drjit.ADMode):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.Float64.eq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Float64.erf_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.erfinv_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.exp2_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.exp_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.floor_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.fma_(self, arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.full_

        (arg0: float, arg1: int) -> drjit.llvm.ad.Float64

    .. py:method:: mitsuba.Float64.gather_(source, index, mask, permute=False)

        Parameter ``source`` (drjit.llvm.ad.Float64):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.ge_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Float64.grad_(self)

        Returns → drjit.llvm.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.grad_enabled_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Float64.gt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Float64.iadd_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.iand_(overloaded)


        .. py:method:: iand_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float64):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

        .. py:method:: iand_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

    .. py:method:: mitsuba.Float64.imod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.imul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.init_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.ior_(overloaded)


        .. py:method:: ior_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float64):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

        .. py:method:: ior_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

    .. py:method:: mitsuba.Float64.is_evaluated_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Float64.is_literal_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Float64.isub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.itruediv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.ixor_(overloaded)


        .. py:method:: ixor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float64):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

        .. py:method:: ixor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

    .. py:method:: mitsuba.Float64.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Float64.le_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Float64.lgamma_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.linspace_

        (arg0: float, arg1: float, arg2: int, arg3: bool) -> drjit.llvm.ad.Float64

    .. py:method:: mitsuba.Float64.load_(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.log2_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.log_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.lt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Float64.map_(ptr, size, callback=None)

        Parameter ``ptr`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Parameter ``callback`` (Callable[[], None]):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.max_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.maximum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.migrate_(self, arg0)

        Parameter ``arg0`` (AllocType):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.min_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.minimum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.mod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.mul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.neg_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.neq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Float64.not_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.opaque_

        (arg0: float, arg1: int) -> drjit.llvm.ad.Float64

    .. py:method:: mitsuba.Float64.or_(overloaded)


        .. py:method:: or_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float64):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

        .. py:method:: or_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

    .. py:method:: mitsuba.Float64.power_(overloaded)


        .. py:method:: power_(self, arg0)

            Parameter ``arg0`` (float):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

        .. py:method:: power_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float64):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

    .. py:method:: mitsuba.Float64.prod_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.rcp_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.reinterpret_array_(overloaded)


        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int64):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt64):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float64):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

    .. py:method:: mitsuba.Float64.resize_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.round_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.rsqrt_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (drjit.llvm.ad.Float64):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.scatter_reduce_(self, op, target, index, mask)

        Parameter ``op`` (ReduceOp):
            *no description available*

        Parameter ``target`` (drjit.llvm.ad.Float64):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.scope_enter_(arg0, arg1)

        Parameter ``arg0`` (drjit.detail.ADScope):
            *no description available*

        Parameter ``arg1`` (List[int]):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.scope_leave_(arg0)

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.sec_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.select_

        (arg0: drjit.llvm.ad.Bool, arg1: drjit.llvm.ad.Float64, arg2: drjit.llvm.ad.Float64) -> drjit.llvm.ad.Float64

    .. py:method:: mitsuba.Float64.set_entry_(self, arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (float):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.set_grad_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.Float64):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.set_grad_enabled_(self, arg0)

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.set_index_ad_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.sin_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.sincos_(self)

        Returns → Tuple[drjit.llvm.ad.Float64, drjit.llvm.ad.Float64]:
            *no description available*

    .. py:method:: mitsuba.Float64.sincosh_(self)

        Returns → Tuple[drjit.llvm.ad.Float64, drjit.llvm.ad.Float64]:
            *no description available*

    .. py:method:: mitsuba.Float64.sinh_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.sqrt_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.sub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.sum_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.tan_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.tanh_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.tgamma_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.traverse_(arg0, arg1)

        Parameter ``arg0`` (drjit.ADMode):
            *no description available*

        Parameter ``arg1`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Float64.truediv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.trunc_(self)

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.Float64.xor_(overloaded)


        .. py:method:: xor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float64):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

        .. py:method:: xor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

    .. py:method:: mitsuba.Float64.zero_

        (arg0: int) -> drjit.llvm.ad.Float64

.. py:class:: mitsuba.Formatter

    Base class: :py:obj:`mitsuba.Object`

    Abstract interface for converting log information into a human-
    readable format

    .. py:method:: __init__(self)


    .. py:method:: mitsuba.Formatter.format(self, level, class_, thread, file, line, msg)

        Turn a log message into a human-readable format

        Parameter ``level`` (:py:obj:`mitsuba.LogLevel`):
            The importance of the debug message

        Parameter ``class_`` (:py:obj:`mitsuba.Class`):
            Originating class or ``nullptr``

        Parameter ``thread`` (mitsuba::Thread):
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


    .. py:method:: __init__(self)

        Construct a new coordinate frame from a single vector

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.Frame3f`):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1, arg2)

        Parameter ``arg0`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Parameter ``arg2`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

    .. py:method:: mitsuba.Frame3f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Frame3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Frame3f.cos_phi(v)

        Give a unit direction, this function returns the cosine of the azimuth
        in a reference spherical coordinate system (see the Frame description)

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Frame3f.cos_phi_2(v)

        Give a unit direction, this function returns the squared cosine of the
        azimuth in a reference spherical coordinate system (see the Frame
        description)

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Frame3f.cos_theta(v)

        Give a unit direction, this function returns the cosine of the
        elevation angle in a reference spherical coordinate system (see the
        Frame description)

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Frame3f.cos_theta_2(v)

        Give a unit direction, this function returns the square cosine of the
        elevation angle in a reference spherical coordinate system (see the
        Frame description)

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Frame3f.sin_phi(v)

        Give a unit direction, this function returns the sine of the azimuth
        in a reference spherical coordinate system (see the Frame description)

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Frame3f.sin_phi_2(v)

        Give a unit direction, this function returns the squared sine of the
        azimuth in a reference spherical coordinate system (see the Frame
        description)

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Frame3f.sin_theta(v)

        Give a unit direction, this function returns the sine of the elevation
        angle in a reference spherical coordinate system (see the Frame
        description)

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Frame3f.sin_theta_2(v)

        Give a unit direction, this function returns the square sine of the
        elevation angle in a reference spherical coordinate system (see the
        Frame description)

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Frame3f.sincos_phi(v)

        Give a unit direction, this function returns the sine and cosine of
        the azimuth in a reference spherical coordinate system (see the Frame
        description)

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Frame3f.sincos_phi_2(v)

        Give a unit direction, this function returns the squared sine and
        cosine of the azimuth in a reference spherical coordinate system (see
        the Frame description)

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Frame3f.tan_theta(v)

        Give a unit direction, this function returns the tangent of the
        elevation angle in a reference spherical coordinate system (see the
        Frame description)

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Frame3f.tan_theta_2(v)

        Give a unit direction, this function returns the square tangent of the
        elevation angle in a reference spherical coordinate system (see the
        Frame description)

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

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

        Parameter ``data`` (numpy.ndarray[numpy.float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][0]]):
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

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Parameter ``data`` (numpy.ndarray[numpy.float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][1]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*


    .. py:method:: mitsuba.Hierarchical2D1.eval(self, pos, param=[0.0], active=True)

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

    .. py:method:: mitsuba.Hierarchical2D1.invert(self, sample, param=[0.0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Hierarchical2D1.sample(self, sample, param=[0.0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Parameter ``data`` (numpy.ndarray[numpy.float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][2]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*


    .. py:method:: mitsuba.Hierarchical2D2.eval(self, pos, param=[0.0, 0.0], active=True)

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

    .. py:method:: mitsuba.Hierarchical2D2.invert(self, sample, param=[0.0, 0.0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Hierarchical2D2.sample(self, sample, param=[0.0, 0.0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Parameter ``data`` (numpy.ndarray[numpy.float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][3]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*


    .. py:method:: mitsuba.Hierarchical2D3.eval(self, pos, param=[0.0, 0.0, 0.0], active=True)

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

    .. py:method:: mitsuba.Hierarchical2D3.invert(self, sample, param=[0.0, 0.0, 0.0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Hierarchical2D3.sample(self, sample, param=[0.0, 0.0, 0.0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Parameter ``rfilter`` (:py:obj:`mitsuba.ReconstructionFilter`):
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

    .. py:method:: __init__(self, tensor, offset=[0, 0], rfilter=None, border=False, normalize=False, coalesce=True, compensate=False, warn_negative=False, warn_invalid=False)

        Parameter ``tensor`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``offset`` (:py:obj:`mitsuba.ScalarPoint2i`):
            *no description available*

        Parameter ``rfilter`` (:py:obj:`mitsuba.ReconstructionFilter`):
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

    .. py:method:: mitsuba.ImageBlock.border_size(self)

        Return the border region used by the reconstruction filter

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.channel_count(self)

        Return the number of channels stored by the image block

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.clear(self)

        Clear the image block contents to zero.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.coalesce(self)

        Try to coalesce reads/writes in JIT modes?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.compensate(self)

        Use Kahan-style error-compensated floating point accumulation?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.has_border(self)

        Does the image block have a border region?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.height(self)

        Return the bitmap's height in pixels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.normalize(self)

        Re-normalize filter weights in put() and read()

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.offset(self)

        Return the current block offset

        Returns → :py:obj:`mitsuba.ScalarPoint2i`:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.put(overloaded)


        .. py:method:: put(self, pos, wavelengths, value, alpha=1.0, weight=1, active=True)

            Accumulate a single sample or a wavefront of samples into the image
            block.

            Parameter ``pos`` (:py:obj:`mitsuba.Point2f`):
                Denotes the sample position in fractional pixel coordinates

            Parameter ``values`` (List[drjit.llvm.ad.Float]):
                Points to an array of length channel_count(), which specifies the
                sample value for each channel.

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

        .. py:method:: put(self, pos, values, active=True)

            Parameter ``pos`` (:py:obj:`mitsuba.Point2f`):
                *no description available*

            Parameter ``active`` (drjit.llvm.ad.Bool):
                Mask to specify active lanes.

    .. py:method:: mitsuba.ImageBlock.put_block(self, block)

        Accumulate a single sample or a wavefront of samples into the image
        block.

        Remark:
            This variant of the put() function assumes that the ImageBlock has
            a standard layout, namely: ``RGB``, potentially ``alpha``, and a
            ``weight`` channel. Use the other variant if the channel
            configuration deviations from this default.

        Parameter ``pos``:
            Denotes the sample position in fractional pixel coordinates

        Parameter ``wavelengths``:
            Sample wavelengths in nanometers

        Parameter ``value``:
            Sample value associated with the specified wavelengths

        Parameter ``alpha``:
            Alpha value associated with the sample

        Parameter ``block`` (:py:obj:`mitsuba.ImageBlock`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.read(self, pos, active=True)

        Parameter ``pos`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.rfilter(self)

        Return the image reconstruction filter underlying the ImageBlock

        Returns → :py:obj:`mitsuba.ReconstructionFilter`:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.set_coalesce(self, arg0)

        Try to coalesce reads/writes in JIT modes?

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.set_compensate(self, arg0)

        Use Kahan-style error-compensated floating point accumulation?

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.set_normalize(self, arg0)

        Re-normalize filter weights in put() and read()

        Parameter ``arg0`` (bool):
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

    .. py:method:: mitsuba.ImageBlock.size(self)

        Return the current block size

        Returns → :py:obj:`mitsuba.ScalarVector2u`:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.tensor(self)

        Return the underlying image tensor

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.warn_invalid(self)

        Warn when writing invalid (NaN, +/- infinity) sample values?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.warn_negative(self)

        Warn when writing negative sample values?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.width(self)

        Return the bitmap's width in pixels

        Returns → int:
            *no description available*

.. py:class:: mitsuba.Int


    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: drjit.llvm.ad.Int, *args) -> None

        Parameter ``arg0`` (drjit.llvm.Int):
            *no description available*

    .. py:method:: mitsuba.Int.abs_(self)

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.add_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.and_(overloaded)


        .. py:method:: and_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

        .. py:method:: and_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

    .. py:method:: mitsuba.Int.andnot_(overloaded)


        .. py:method:: andnot_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

        .. py:method:: andnot_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

    .. py:method:: mitsuba.Int.arange_

        (arg0: int, arg1: int, arg2: int) -> drjit.llvm.ad.Int

    .. py:method:: mitsuba.Int.assign(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int.block_sum_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.copy_(self)

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.data_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Int.detach_(self)

        Returns → drjit.llvm.Int:
            *no description available*

    .. py:method:: mitsuba.Int.detach_ref_(self)

        Returns → drjit.llvm.Int:
            *no description available*

    .. py:method:: mitsuba.Int.dot_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Int.eq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Int.floordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.fma_(self, arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.full_

        (arg0: int, arg1: int) -> drjit.llvm.ad.Int

    .. py:method:: mitsuba.Int.gather_(source, index, mask, permute=False)

        Parameter ``source`` (drjit.llvm.ad.Int):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.ge_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Int.gt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Int.iadd_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.iand_(overloaded)


        .. py:method:: iand_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

        .. py:method:: iand_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

    .. py:method:: mitsuba.Int.ifloordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.imod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.imul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.init_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int.ior_(overloaded)


        .. py:method:: ior_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

        .. py:method:: ior_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

    .. py:method:: mitsuba.Int.is_evaluated_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Int.is_literal_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Int.isl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.isr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.isub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.ixor_(overloaded)


        .. py:method:: ixor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

        .. py:method:: ixor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

    .. py:method:: mitsuba.Int.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Int.le_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Int.linspace_

        (arg0: int, arg1: int, arg2: int, arg3: bool) -> drjit.llvm.ad.Int

    .. py:method:: mitsuba.Int.load_(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.lt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Int.lzcnt_(self)

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.map_(ptr, size, callback=None)

        Parameter ``ptr`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Parameter ``callback`` (Callable[[], None]):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.max_(self)

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.maximum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.migrate_(self, arg0)

        Parameter ``arg0`` (AllocType):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.min_(self)

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.minimum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.mod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.mul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.mulhi_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.neg_(self)

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.neq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Int.not_(self)

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.opaque_

        (arg0: int, arg1: int) -> drjit.llvm.ad.Int

    .. py:method:: mitsuba.Int.or_(overloaded)


        .. py:method:: or_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

        .. py:method:: or_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

    .. py:method:: mitsuba.Int.popcnt_(self)

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.prod_(self)

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.reinterpret_array_(overloaded)


        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

    .. py:method:: mitsuba.Int.resize_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (drjit.llvm.ad.Int):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int.scatter_reduce_(self, op, target, index, mask)

        Parameter ``op`` (ReduceOp):
            *no description available*

        Parameter ``target`` (drjit.llvm.ad.Int):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int.select_

        (arg0: drjit.llvm.ad.Bool, arg1: drjit.llvm.ad.Int, arg2: drjit.llvm.ad.Int) -> drjit.llvm.ad.Int

    .. py:method:: mitsuba.Int.set_entry_(self, arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int.set_index_ad_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int.sl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.sr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.sub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.sum_(self)

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.tzcnt_(self)

        Returns → drjit.llvm.ad.Int:
            *no description available*

    .. py:method:: mitsuba.Int.xor_(overloaded)


        .. py:method:: xor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

        .. py:method:: xor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int:
                *no description available*

    .. py:method:: mitsuba.Int.zero_

        (arg0: int) -> drjit.llvm.ad.Int

.. py:class:: mitsuba.Int64


    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: drjit.llvm.ad.Int64, *args) -> None

        Parameter ``arg0`` (drjit.llvm.Int64):
            *no description available*

    .. py:method:: mitsuba.Int64.abs_(self)

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.add_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.and_(overloaded)


        .. py:method:: and_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int64):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

        .. py:method:: and_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

    .. py:method:: mitsuba.Int64.andnot_(overloaded)


        .. py:method:: andnot_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int64):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

        .. py:method:: andnot_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

    .. py:method:: mitsuba.Int64.arange_

        (arg0: int, arg1: int, arg2: int) -> drjit.llvm.ad.Int64

    .. py:method:: mitsuba.Int64.assign(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int64.block_sum_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.copy_(self)

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.data_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Int64.detach_(self)

        Returns → drjit.llvm.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.detach_ref_(self)

        Returns → drjit.llvm.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.dot_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Int64.eq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Int64.floordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.fma_(self, arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.full_

        (arg0: int, arg1: int) -> drjit.llvm.ad.Int64

    .. py:method:: mitsuba.Int64.gather_(source, index, mask, permute=False)

        Parameter ``source`` (drjit.llvm.ad.Int64):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.ge_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Int64.gt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Int64.iadd_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.iand_(overloaded)


        .. py:method:: iand_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int64):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

        .. py:method:: iand_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

    .. py:method:: mitsuba.Int64.ifloordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.imod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.imul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.init_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int64.ior_(overloaded)


        .. py:method:: ior_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int64):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

        .. py:method:: ior_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

    .. py:method:: mitsuba.Int64.is_evaluated_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Int64.is_literal_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Int64.isl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.isr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.isub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.ixor_(overloaded)


        .. py:method:: ixor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int64):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

        .. py:method:: ixor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

    .. py:method:: mitsuba.Int64.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Int64.le_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Int64.linspace_

        (arg0: int, arg1: int, arg2: int, arg3: bool) -> drjit.llvm.ad.Int64

    .. py:method:: mitsuba.Int64.load_(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.lt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Int64.lzcnt_(self)

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.map_(ptr, size, callback=None)

        Parameter ``ptr`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Parameter ``callback`` (Callable[[], None]):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.max_(self)

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.maximum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.migrate_(self, arg0)

        Parameter ``arg0`` (AllocType):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.min_(self)

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.minimum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.mod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.mul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.mulhi_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.neg_(self)

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.neq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Int64.not_(self)

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.opaque_

        (arg0: int, arg1: int) -> drjit.llvm.ad.Int64

    .. py:method:: mitsuba.Int64.or_(overloaded)


        .. py:method:: or_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int64):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

        .. py:method:: or_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

    .. py:method:: mitsuba.Int64.popcnt_(self)

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.prod_(self)

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.reinterpret_array_(overloaded)


        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int64):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt64):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float64):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

    .. py:method:: mitsuba.Int64.resize_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int64.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (drjit.llvm.ad.Int64):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int64.scatter_reduce_(self, op, target, index, mask)

        Parameter ``op`` (ReduceOp):
            *no description available*

        Parameter ``target`` (drjit.llvm.ad.Int64):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int64.select_

        (arg0: drjit.llvm.ad.Bool, arg1: drjit.llvm.ad.Int64, arg2: drjit.llvm.ad.Int64) -> drjit.llvm.ad.Int64

    .. py:method:: mitsuba.Int64.set_entry_(self, arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int64.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int64.set_index_ad_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int64.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Int64.sl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.sr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.sub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.sum_(self)

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.tzcnt_(self)

        Returns → drjit.llvm.ad.Int64:
            *no description available*

    .. py:method:: mitsuba.Int64.xor_(overloaded)


        .. py:method:: xor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int64):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

        .. py:method:: xor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Int64:
                *no description available*

    .. py:method:: mitsuba.Int64.zero_

        (arg0: int) -> drjit.llvm.ad.Int64

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

    .. py:method:: mitsuba.Integrator.aov_names(self)

        For integrators that return one or more arbitrary output variables
        (AOVs), this function specifies a list of associated channel names.
        The default implementation simply returns an empty vector.

        Returns → List[str]:
            *no description available*

    .. py:method:: mitsuba.Integrator.cancel(self)

        Cancel a running render job (e.g. after receiving Ctrl-C)

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Integrator.render(overloaded)


        .. py:method:: render(self, scene, sensor, seed=0, spp=0, develop=True, evaluate=True)

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

            Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
                *no description available*

            Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
                *no description available*

            Returns → drjit.llvm.ad.TensorXf:
                *no description available*

        .. py:method:: render(self, scene, sensor=0, seed=0, spp=0, develop=True, evaluate=True)

            Render the scene

            This function is just a thin wrapper around the previous render()
            overload. It accepts a sensor *index* instead and renders the scene
            using sensor 0 by default.

            Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
                *no description available*

            Parameter ``sensor`` (int):
                *no description available*

            Parameter ``seed`` (int):
                *no description available*

            Parameter ``spp`` (int):
                *no description available*

            Parameter ``develop`` (bool):
                *no description available*

            Parameter ``evaluate`` (bool):
                *no description available*

            Returns → drjit.llvm.ad.TensorXf:
                *no description available*

    .. py:method:: mitsuba.Integrator.render_backward(scene, params, grad_in, sensor=0, seed=0, spp=0)

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
        to ``mi.traverse()``. Use ``dr.grad()`` to query the resulting gradients of
        these parameters once ``render_backward()`` returns.

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
            The scene to be rendered differentially.

        Parameter ``params`` (Any):
           An arbitrary container of scene parameters that should receive
           gradients. Typically this will be an instance of type
           ``mi.SceneParameters`` obtained via ``mi.traverse()``. However, it could
           also be a Python list/dict/object tree (DrJit will traverse it to find
           all parameters). Gradient tracking must be explicitly enabled for each of
           these parameters using ``dr.enable_grad(params['parameter_name'])`` (i.e.
           ``render_backward()`` will not do this for you).

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

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Integrator.render_forward(scene, params, sensor=0, seed=0, spp=0)

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
        that can be obtained obtained via a call to ``mi.traverse()``.

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
            The scene to be rendered differentially.

        Parameter ``params`` (Any):
           An arbitrary container of scene parameters that should receive
           gradients. Typically this will be an instance of type
           ``mi.SceneParameters`` obtained via ``mi.traverse()``. However, it could
           also be a Python list/dict/object tree (DrJit will traverse it to find
           all parameters). Gradient tracking must be explicitly enabled for each of
           these parameters using ``dr.enable_grad(params['parameter_name'])`` (i.e.
           ``render_forward()`` will not do this for you). Furthermore,
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

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → mi.TensorXf:
            *no description available*

    .. py:method:: mitsuba.Integrator.should_stop(self)

        Indicates whether cancel() or a timeout have occurred. Should be
        checked regularly in the integrator's main loop so that timeouts are
        enforced accurately.

        Note that accurate timeouts rely on m_render_timer, which needs to be
        reset at the beginning of the rendering phase.

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.Interaction3f

    Generic surface interaction data structure


    .. py:method:: __init__(self)

        Constructor

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

    .. py:method:: __init__(self, t, time, wavelengths, p, n=0)

        Parameter ``t`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``time`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``wavelengths`` (:py:obj:`mitsuba.Color0f`):
            *no description available*

        Parameter ``p`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``n`` (:py:obj:`mitsuba.Normal3f`):
            *no description available*

    .. py:method:: mitsuba.Interaction3f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Interaction3f.is_valid(self)

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

    .. py:method:: mitsuba.Interaction3f.zero_(overloaded)


        .. py:method:: zero_(self, size=1)

            Parameter ``size`` (int):
                *no description available*

        .. py:method:: zero_(self, arg0)

            This callback method is invoked by dr::zeros<>, and takes care of
            fields that deviate from the standard zero-initialization convention.
            In this particular class, the ``t`` field should be set to an infinite
            value to mark invalid intersection records.

            Parameter ``arg0`` (int):
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


    .. py:method:: __init__(self)

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

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.IrregularContinuousDistribution`):
            *no description available*

    .. py:method:: __init__(self, nodes, pdf)

        Initialize from a given density function discretized on nodes
        ``nodes``

        Parameter ``nodes`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``pdf`` (drjit.llvm.ad.Float):
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.cdf(self)

        Return the unnormalized discrete cumulative distribution function over
        intervals

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.empty(self)

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

    .. py:method:: mitsuba.IrregularContinuousDistribution.integral(self)

        Return the original integral of PDF entries before normalization

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.interval_resolution(self)

        Return the minimum resolution of the discretization

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.max(self)

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.nodes(self)

        Return the nodes of the underlying discretization

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.normalization(self)

        Return the normalization factor (i.e. the inverse of sum())

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.pdf(self)

        Return the unnormalized discretized probability density function

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.range(self)

        Return the range of the distribution

        Returns → drjit.scalar.Array2f:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.sample(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``value`` (drjit.llvm.ad.Float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The sampled position.

    .. py:method:: mitsuba.IrregularContinuousDistribution.sample_pdf(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``value`` (drjit.llvm.ad.Float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            A tuple consisting of

        1. the sampled position. 2. the normalized probability density of the
        sample.

    .. py:method:: mitsuba.IrregularContinuousDistribution.size(self)

        Return the number of discretizations

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.update(self)

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

    Members:

    .. py:data:: Trace



    .. py:data:: Debug

        Trace message, for extremely verbose debugging

    .. py:data:: Info

        Debug message, usually turned off

    .. py:data:: Warn

        More relevant debug / information message

    .. py:data:: Error

        Warning message

    .. py:method:: __init__(self, value)

        Parameter ``value`` (int):
            *no description available*


    .. py:method:: mitsuba.LogLevel.name
        :property:

.. py:class:: mitsuba.Logger

    Base class: :py:obj:`mitsuba.Object`

    Responsible for processing log messages

    Upon receiving a log message, the Logger class invokes a Formatter to
    convert it into a human-readable form. Following that, it sends this
    information to every registered Appender.

    .. py:method:: __init__(self, arg0)

        Construct a new logger with the given minimum log level

        Parameter ``arg0`` (:py:obj:`mitsuba.LogLevel`):
            *no description available*


    .. py:method:: mitsuba.Logger.add_appender(self, arg0)

        Add an appender to this logger

        Parameter ``arg0`` (:py:obj:`mitsuba.Appender`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Logger.appender(self, arg0)

        Return one of the appenders

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.Appender`:
            *no description available*

    .. py:method:: mitsuba.Logger.appender_count(self)

        Return the number of registered appenders

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Logger.clear_appenders(self)

        Remove all appenders from this logger

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Logger.error_level(self)

        Return the current error level

        Returns → :py:obj:`mitsuba.LogLevel`:
            *no description available*

    .. py:method:: mitsuba.Logger.formatter(self)

        Return the logger's formatter implementation

        Returns → :py:obj:`mitsuba.Formatter`:
            *no description available*

    .. py:method:: mitsuba.Logger.log_level(self)

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

        Parameter ``ptr`` (capsule):
            Custom pointer payload. This is used to express the context of a
            progress message. When rendering a scene, it will usually contain
            a pointer to the associated ``RenderJob``.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Logger.read_log(self)

        Return the contents of the log file as a string

        Throws a runtime exception upon failure

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Logger.remove_appender(self, arg0)

        Remove an appender from this logger

        Parameter ``arg0`` (:py:obj:`mitsuba.Appender`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Logger.set_error_level(self, arg0)

        Set the error log level (this level and anything above will throw
        exceptions).

        The value provided here can be used for instance to turn warnings into
        errors. But *level* must always be less than Error, i.e. it isn't
        possible to cause errors not to throw an exception.

        Parameter ``arg0`` (:py:obj:`mitsuba.LogLevel`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Logger.set_formatter(self, arg0)

        Set the logger's formatter implementation

        Parameter ``arg0`` (:py:obj:`mitsuba.Formatter`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Logger.set_log_level(self, arg0)

        Set the log level (everything below will be ignored)

        Parameter ``arg0`` (:py:obj:`mitsuba.LogLevel`):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.Loop

    .. py:method:: mitsuba.Loop.__call__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Loop.init(self)

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Loop.put(self, arg0)

        Parameter ``arg0`` (function):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Loop.set_eval_stride(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Loop.set_max_iterations(self, arg0)

        Parameter ``arg0`` (int):
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
    :value: 3.3.0

.. py:data:: mitsuba.MI_VERSION_MAJOR
    :type: int
    :value: 3

.. py:data:: mitsuba.MI_VERSION_MINOR
    :type: int
    :value: 3

.. py:data:: mitsuba.MI_VERSION_PATCH
    :type: int
    :value: 0

.. py:data:: mitsuba.MI_YEAR
    :type: str
    :value: 2022

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

        Parameter ``data`` (numpy.ndarray[numpy.float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][0]]):
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

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Parameter ``data`` (numpy.ndarray[numpy.float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][1]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*


    .. py:method:: mitsuba.MarginalContinuous2D1.eval(self, pos, param=[0.0], active=True)

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

    .. py:method:: mitsuba.MarginalContinuous2D1.invert(self, sample, param=[0.0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalContinuous2D1.sample(self, sample, param=[0.0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Parameter ``data`` (numpy.ndarray[numpy.float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][2]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*


    .. py:method:: mitsuba.MarginalContinuous2D2.eval(self, pos, param=[0.0, 0.0], active=True)

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

    .. py:method:: mitsuba.MarginalContinuous2D2.invert(self, sample, param=[0.0, 0.0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalContinuous2D2.sample(self, sample, param=[0.0, 0.0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Parameter ``data`` (numpy.ndarray[numpy.float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][3]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*


    .. py:method:: mitsuba.MarginalContinuous2D3.eval(self, pos, param=[0.0, 0.0, 0.0], active=True)

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

    .. py:method:: mitsuba.MarginalContinuous2D3.invert(self, sample, param=[0.0, 0.0, 0.0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalContinuous2D3.sample(self, sample, param=[0.0, 0.0, 0.0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Parameter ``data`` (numpy.ndarray[numpy.float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][0]]):
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

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Parameter ``data`` (numpy.ndarray[numpy.float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][1]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*


    .. py:method:: mitsuba.MarginalDiscrete2D1.eval(self, pos, param=[0.0], active=True)

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

    .. py:method:: mitsuba.MarginalDiscrete2D1.invert(self, sample, param=[0.0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalDiscrete2D1.sample(self, sample, param=[0.0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Parameter ``data`` (numpy.ndarray[numpy.float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][2]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*


    .. py:method:: mitsuba.MarginalDiscrete2D2.eval(self, pos, param=[0.0, 0.0], active=True)

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

    .. py:method:: mitsuba.MarginalDiscrete2D2.invert(self, sample, param=[0.0, 0.0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalDiscrete2D2.sample(self, sample, param=[0.0, 0.0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Parameter ``data`` (numpy.ndarray[numpy.float32]):
            *no description available*

        Parameter ``param_values`` (List[List[float][3]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*


    .. py:method:: mitsuba.MarginalDiscrete2D3.eval(self, pos, param=[0.0, 0.0, 0.0], active=True)

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

    .. py:method:: mitsuba.MarginalDiscrete2D3.invert(self, sample, param=[0.0, 0.0, 0.0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalDiscrete2D3.sample(self, sample, param=[0.0, 0.0, 0.0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.Matrix2f

    .. py:method:: mitsuba.Matrix2f.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Array2f:
            *no description available*

    .. py:method:: mitsuba.Matrix2f.entry_ref_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Array2f:
            *no description available*

    .. py:method:: mitsuba.Matrix2f.set_entry_(self, arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Array2f):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.Matrix3f

    .. py:method:: mitsuba.Matrix3f.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Array3f:
            *no description available*

    .. py:method:: mitsuba.Matrix3f.entry_ref_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Array3f:
            *no description available*

    .. py:method:: mitsuba.Matrix3f.set_entry_(self, arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Array3f):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Matrix3f.sh_eval_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → List[drjit.llvm.ad.Array3f]:
            *no description available*

.. py:class:: mitsuba.Matrix4f

    .. py:method:: mitsuba.Matrix4f.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Array4f:
            *no description available*

    .. py:method:: mitsuba.Matrix4f.entry_ref_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Array4f:
            *no description available*

    .. py:method:: mitsuba.Matrix4f.set_entry_(self, arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Array4f):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.Medium

    Base class: :py:obj:`mitsuba.Object`

    .. py:method:: mitsuba.Medium.transmittance_eval_pdf(self, mi, active)

        Compute the transmittance and PDF

        This function evaluates the transmittance and PDF of sampling a
        certain free-flight distance The returned PDF takes into account if a
        medium interaction occurred (mi.t <= si.t) or the ray left the medium
        (mi.t > si.t)

        The evaluated PDF is spectrally varying. This allows to account for
        the fact that the free-flight distance sampling distribution can
        depend on the wavelength.

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`]:
            This method returns a pair of (Transmittance, PDF).

    .. py:method:: mitsuba.Medium.get_majorant(self, mi, active=True)

        Returns the medium's majorant used for delta tracking

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            *no description available*

    .. py:method:: mitsuba.Medium.get_scattering_coefficients(self, mi, active=True)

        Returns the medium coefficients Sigma_s, Sigma_n and Sigma_t evaluated
        at a given MediumInteraction mi

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`]:
            *no description available*

    .. py:method:: mitsuba.Medium.has_spectral_extinction(self)

        Returns whether this medium has a spectrally varying extinction

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Medium.id(self)

        Return a string identifier

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Medium.intersect_aabb(self, ray)

        Intersects a ray with the medium's bounding box

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Returns → Tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Medium.is_homogeneous(self)

        Returns whether this medium is homogeneous

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Medium.phase_function(self)

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

        Returns → :py:obj:`mitsuba.MediumInteraction`:
            This method returns a MediumInteraction. The MediumInteraction
            will always be valid, except if the ray missed the Medium's
            bounding box.

    .. py:method:: mitsuba.Medium.use_emitter_sampling(self)

        Returns whether this specific medium instance uses emitter sampling

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.MediumInteraction3f

    Base class: :py:obj:`mitsuba.Interaction3f`

    Stores information related to a medium scattering interaction


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.MediumInteraction3f`):
            *no description available*

    .. py:method:: mitsuba.MediumInteraction3f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.MediumInteraction3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.MediumInteraction3f.medium
        :property:

        Pointer to the associated medium

    .. py:method:: mitsuba.MediumInteraction3f.mint
        :property:

        mint used when sampling the given distance ``t``

    .. py:method:: mitsuba.MediumInteraction3f.sh_frame
        :property:

        Shading frame

    .. py:method:: mitsuba.MediumInteraction3f.to_local(self, v)

        Convert a world-space vector into local shading coordinates

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

    .. py:method:: mitsuba.MediumInteraction3f.to_world(self, v)

        Convert a local shading-space vector into world space

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

    .. py:method:: mitsuba.MediumInteraction3f.wi
        :property:

        Incident direction in the local shading frame

.. py:class:: mitsuba.MediumPtr


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Medium`):
            *no description available*

    .. py:method:: mitsuba.MediumPtr.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.MediumPtr`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.Medium`:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.eq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.MediumPtr`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.transmittance_eval_pdf(self, mi, active)

        Compute the transmittance and PDF

        This function evaluates the transmittance and PDF of sampling a
        certain free-flight distance The returned PDF takes into account if a
        medium interaction occurred (mi.t <= si.t) or the ray left the medium
        (mi.t > si.t)

        The evaluated PDF is spectrally varying. This allows to account for
        the fact that the free-flight distance sampling distribution can
        depend on the wavelength.

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`]:
            This method returns a pair of (Transmittance, PDF).

    .. py:method:: mitsuba.MediumPtr.gather_(source, index, mask, permute=False)

        Parameter ``source`` (:py:obj:`mitsuba.MediumPtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → :py:obj:`mitsuba.MediumPtr`:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.get_majorant(self, mi, active=True)

        Returns the medium's majorant used for delta tracking

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.get_scattering_coefficients(self, mi, active=True)

        Returns the medium coefficients Sigma_s, Sigma_n and Sigma_t evaluated
        at a given MediumInteraction mi

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`]:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.has_spectral_extinction(self)

        Returns whether this medium has a spectrally varying extinction

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.intersect_aabb(self, ray)

        Intersects a ray with the medium's bounding box

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Returns → Tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.is_homogeneous(self)

        Returns whether this medium is homogeneous

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.neq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.MediumPtr`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.phase_function(self)

        Return the phase function of this medium

        Returns → drjit::DiffArray<drjit::LLVMArray<:py:obj:`mitsuba.PhaseFunction` const*>>:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.registry_get_max_()

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.registry_get_ptr_(arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.reinterpret_array_(arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → :py:obj:`mitsuba.MediumPtr`:
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

        Returns → :py:obj:`mitsuba.MediumInteraction`:
            This method returns a MediumInteraction. The MediumInteraction
            will always be valid, except if the ray missed the Medium's
            bounding box.

    .. py:method:: mitsuba.MediumPtr.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (:py:obj:`mitsuba.MediumPtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.select_(arg0, arg1, arg2)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.MediumPtr`):
            *no description available*

        Parameter ``arg2`` (:py:obj:`mitsuba.MediumPtr`):
            *no description available*

        Returns → :py:obj:`mitsuba.MediumPtr`:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.use_emitter_sampling(self)

        Returns whether this specific medium instance uses emitter sampling

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.zero_

        (arg0: int) -> :py:obj:`mitsuba.llvm_ad_rgb.MediumPtr`

.. py:class:: mitsuba.MemoryMappedFile

    Base class: :py:obj:`mitsuba.Object`

    Basic cross-platform abstraction for memory mapped files

    Remark:
        The Python API has one additional constructor
        <tt>MemoryMappedFile(filename, array)<tt>, which creates a new
        file, maps it into memory, and copies the array contents.


    .. py:method:: __init__(self, filename, size)

        Create a new memory-mapped file of the specified size

        Parameter ``filename`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

    .. py:method:: __init__(self, filename, write=False)

        Map the specified file into memory

        Parameter ``filename`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Parameter ``write`` (bool):
            *no description available*

    .. py:method:: __init__(self, filename, array)

        Parameter ``filename`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Parameter ``array`` (numpy.ndarray):
            *no description available*

    .. py:method:: mitsuba.MemoryMappedFile.can_write(self)

        Return whether the mapped memory region can be modified

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.MemoryMappedFile.create_temporary(arg0)

        Create a temporary memory-mapped file

        Remark:
            When closing the mapping, the file is automatically deleted.
            Mitsuba additionally informs the OS that any outstanding changes
            that haven't yet been written to disk can be discarded (Linux/OSX
            only).

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.MemoryMappedFile`:
            *no description available*

    .. py:method:: mitsuba.MemoryMappedFile.data(self)

        Return a pointer to the file contents in memory

        Returns → capsule:
            *no description available*

    .. py:method:: mitsuba.MemoryMappedFile.filename(self)

        Return the associated filename

        Returns → :py:obj:`mitsuba.filesystem.path`:
            *no description available*

    .. py:method:: mitsuba.MemoryMappedFile.resize(self, arg0)

        Resize the memory-mapped file

        This involves remapping the file, which will generally change the
        pointer obtained via data()

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.MemoryMappedFile.size(self)

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


    .. py:method:: mitsuba.MemoryStream.capacity(self)

        Return the current capacity of the underlying memory buffer

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.MemoryStream.owns_buffer(self)

        Return whether or not the memory stream owns the underlying buffer

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.MemoryStream.raw_buffer(self)

        Returns → bytes:
            *no description available*

.. py:class:: mitsuba.Mesh

    Base class: :py:obj:`mitsuba.Shape`

    Overloaded function.

    1. __init__(self: :py:obj:`mitsuba.llvm_ad_rgb.Mesh`, props: :py:obj:`mitsuba.llvm_ad_rgb.Properties`) -> None

    2. __init__(self: :py:obj:`mitsuba.llvm_ad_rgb.Mesh`, name: str, vertex_count: int, face_count: int, props: :py:obj:`mitsuba.llvm_ad_rgb.Properties` = Properties(), has_vertex_normals: bool = False, has_vertex_texcoords: bool = False) -> None

    Create a new mesh with the given vertex and face data structures

    .. py:method:: mitsuba.Mesh.add_attribute(self, name, size, buffer)

        Add an attribute buffer with the given ``name`` and ``dim``

        Parameter ``name`` (str):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Parameter ``buffer`` (List[float]):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Mesh.face_count(self)

        Return the total number of faces

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Mesh.face_indices(self, index, active=True)

        Returns the face indices associated with triangle ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Array3u:
            *no description available*

    .. py:method:: mitsuba.Mesh.has_vertex_normals(self)

        Does this mesh have per-vertex normals?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Mesh.has_vertex_texcoords(self)

        Does this mesh have per-vertex texture coordinates?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Mesh.initialize(self)

        Must be called at the end of the constructor of Mesh plugins

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Mesh.ray_intersect_triangle(self, index, ray, active=True)

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.PreliminaryIntersection`:
            *no description available*

    .. py:method:: mitsuba.Mesh.vertex_count(self)

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

    .. py:method:: mitsuba.Mesh.vertex_position(self, index, active=True)

        Returns the world-space position of the vertex with index ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Point3f`:
            *no description available*

    .. py:method:: mitsuba.Mesh.vertex_texcoord(self, index, active=True)

        Returns the UV texture coordinates of the vertex with index ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Point2f`:
            *no description available*

    .. py:method:: mitsuba.Mesh.write_ply(overloaded)


        .. py:method:: write_ply(self, filename)

            Write the mesh to a binary PLY file

            Parameter ``filename`` (str):
                Target file path on disk

        .. py:method:: write_ply(self, stream)

            Write the mesh encoded in binary PLY format to a stream

            Parameter ``stream`` (:py:obj:`mitsuba.Stream`):
                Target stream that will receive the encoded output

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

    .. py:method:: __init__(self, type, alpha_u, alpha_v, sample_visible=True)

        Parameter ``type`` (:py:obj:`mitsuba.MicrofacetType`):
            *no description available*

        Parameter ``alpha_u`` (float):
            *no description available*

        Parameter ``alpha_v`` (float):
            *no description available*

        Parameter ``sample_visible`` (bool):
            *no description available*

    .. py:method:: __init__(self, type, alpha, sample_visible=True)

        Parameter ``type`` (:py:obj:`mitsuba.MicrofacetType`):
            *no description available*

        Parameter ``alpha`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``sample_visible`` (bool):
            *no description available*

    .. py:method:: __init__(self, type, alpha_u, alpha_v, sample_visible=True)

        Parameter ``type`` (:py:obj:`mitsuba.MicrofacetType`):
            *no description available*

        Parameter ``alpha_u`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``alpha_v`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``sample_visible`` (bool):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Properties`):
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

    .. py:method:: mitsuba.MicrofacetDistribution.alpha(self)

        Return the roughness (isotropic case)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.alpha_u(self)

        Return the roughness along the tangent direction

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.alpha_v(self)

        Return the roughness along the bitangent direction

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.eval(self, m)

        Evaluate the microfacet distribution function

        Parameter ``m`` (:py:obj:`mitsuba.Vector3f`):
            The microfacet normal

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.is_anisotropic(self)

        Is this an anisotropic microfacet distribution?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.is_isotropic(self)

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

        Returns → Tuple[:py:obj:`mitsuba.Normal3f`, drjit.llvm.ad.Float]:
            A tuple consisting of the sampled microfacet normal and the
            associated solid angle density

    .. py:method:: mitsuba.MicrofacetDistribution.sample_visible(self)

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

    .. py:method:: mitsuba.MicrofacetDistribution.type(self)

        Return the distribution type

        Returns → :py:obj:`mitsuba.MicrofacetType`:
            *no description available*

.. py:class:: mitsuba.MicrofacetType

    Supported normal distribution functions

    Members:

    .. py:data:: Beckmann

        Beckmann distribution derived from Gaussian random surfaces

    .. py:data:: GGX

        GGX: Long-tailed distribution for very rough surfaces (aka.
        Trowbridge-Reitz distr.)

    .. py:method:: __init__(self, value)

        Parameter ``value`` (int):
            *no description available*


    .. py:method:: mitsuba.MicrofacetType.name
        :property:

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
    implementation that only adds 32 bits to the base object (for the
    counter) and has no overhead for references.


    .. py:method:: __init__(self)

        Default constructor

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.Object`):
            *no description available*

    .. py:method:: mitsuba.Object.class_(self)

        Return a Class instance containing run-time type information about
        this Object

        See also:
            Class

        Returns → :py:obj:`mitsuba.Class`:
            *no description available*

    .. py:method:: mitsuba.Object.dec_ref(self, dealloc=True)

        Decrease the reference count of the object and possibly deallocate it.

        The object will automatically be deallocated once the reference count
        reaches zero.

        Parameter ``dealloc`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Object.expand(self)

        Expand the object into a list of sub-objects and return them

        In some cases, an Object instance is merely a container for a number
        of sub-objects. In the context of Mitsuba, an example would be a
        combined sun & sky emitter instantiated via XML, which recursively
        expands into a separate sun & sky instance. This functionality is
        supported by any Mitsuba object, hence it is located this level.

        Returns → list:
            *no description available*

    .. py:method:: mitsuba.Object.id(self)

        Return an identifier of the current instance (if available)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Object.inc_ref(self)

        Increase the object's reference count by one

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Object.parameters_changed(self, keys=[])

        Update internal state after applying changes to parameters

        This function should be invoked when attributes (obtained via
        traverse) are modified in some way. The object can then update its
        internal state so that derived quantities are consistent with the
        change.

        Parameter ``keys`` (List[str]):
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

    .. py:method:: mitsuba.Object.ref_count(self)

        Return the current reference count

        Returns → int:
            *no description available*

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


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Object`):
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.ObjectPtr`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.Object`:
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.eq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.ObjectPtr`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.gather_(source, index, mask, permute=False)

        Parameter ``source`` (:py:obj:`mitsuba.ObjectPtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → :py:obj:`mitsuba.ObjectPtr`:
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.neq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.ObjectPtr`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.registry_get_max_()

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.registry_get_ptr_(arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.reinterpret_array_(arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → :py:obj:`mitsuba.ObjectPtr`:
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (:py:obj:`mitsuba.ObjectPtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.select_(arg0, arg1, arg2)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.ObjectPtr`):
            *no description available*

        Parameter ``arg2`` (:py:obj:`mitsuba.ObjectPtr`):
            *no description available*

        Returns → :py:obj:`mitsuba.ObjectPtr`:
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ObjectPtr.zero_

        (arg0: int) -> :py:obj:`mitsuba.llvm_ad_rgb.ObjectPtr`

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


    .. py:method:: mitsuba.OptixDenoiser.__call__(overloaded)


        .. py:method:: __call__(self, noisy, denoise_alpha=True, albedo=[], normals=[], to_sensor=None, flow=[], previous_denoised=[])

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

            Parameter ``to_sensor`` (object):
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

        .. py:method:: __call__(self, noisy, denoise_alpha=True, albedo_ch='', normals_ch='', to_sensor=None, flow_ch='', previous_denoised_ch='', noisy_ch='<root>')

            Apply denoiser on inputs which are Bitmap objects.

            Parameter ``noisy`` (:py:obj:`mitsuba.Bitmap`):
                The noisy input. When passing additional information like albedo
                or normals to the denoiser, this Bitmap object must be a
                MultiChannel bitmap.

            Parameter ``denoise_alpha`` (bool):
                Whether or not the alpha channel (if specified in the noisy input)
                should be denoised too. This parameter is optional, by default it
                is true.

            Parameter ``albedo_ch`` (str):
                The name of the channel in the ``noisy`` parameter which contains
                the albedo information of the noisy rendering. This parameter is
                optional unless the OptixDenoiser was built with albedo support.

            Parameter ``normals_ch`` (str):
                The name of the channel in the ``noisy`` parameter which contains
                the shading normal information of the noisy rendering. The normals
                must be in the coordinate frame of the sensor which was used to
                render the noisy input. This parameter is optional unless the
                OptixDenoiser was built with normals support.

            Parameter ``to_sensor`` (object):
                A Transform4f which is applied to the ``normals`` parameter before
                denoising. This should be used to transform the normals into the
                correct coordinate frame. This parameter is optional, by default
                no transformation is applied.

            Parameter ``flow_ch`` (str):
                With temporal denoising, this parameter is name of the channel in
                the ``noisy`` parameter which contains the optical flow between
                the previous frame and the current one. It should capture the 2D
                motion of each individual pixel. When this parameter is unknown,
                it can been set to a zero-initialized TensorXf of the correct size
                and still produce convincing results. This parameter is optional
                unless the OptixDenoiser was built with temporal denoising
                support.

            Parameter ``previous_denoised_ch`` (str):
                With temporal denoising, this parameter is name of the channel in
                the ``noisy`` parameter which contains the previous denoised
                frame. For the very first frame, the OptiX documentation
                recommends passing the noisy input for this argument. This
                parameter is optional unless the OptixDenoiser was built with
                temporal denoising support.

            Parameter ``noisy_ch`` (str):
                The name of the channel in the ``noisy`` parameter which contains
                the shading normal information of the noisy rendering.

            Returns → :py:obj:`mitsuba.Bitmap`:
                The denoised input.

.. py:class:: mitsuba.PCG32


    .. py:method:: __init__(self, size=1, initstate=9600629759793949339, initseq=15726070495360670683)

        Parameter ``size`` (int):
            *no description available*

        Parameter ``initstate`` (drjit.llvm.ad.UInt64):
            *no description available*

        Parameter ``initseq`` (drjit.llvm.ad.UInt64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.PCG32):
            *no description available*

    .. py:method:: mitsuba.PCG32.next_float32(overloaded)


        .. py:method:: next_float32(self)

            Returns → drjit.llvm.ad.Float:
                *no description available*

        .. py:method:: next_float32(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float:
                *no description available*

    .. py:method:: mitsuba.PCG32.next_float64(overloaded)


        .. py:method:: next_float64(self)

            Returns → drjit.llvm.ad.Float64:
                *no description available*

        .. py:method:: next_float64(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.Float64:
                *no description available*

    .. py:method:: mitsuba.PCG32.next_uint32(overloaded)


        .. py:method:: next_uint32(self)

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: next_uint32(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

    .. py:method:: mitsuba.PCG32.next_uint32_bounded(self, bound, mask=True)

        Parameter ``bound`` (int):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.PCG32.next_uint64(overloaded)


        .. py:method:: next_uint64(self)

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

        .. py:method:: next_uint64(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

    .. py:method:: mitsuba.PCG32.next_uint64_bounded(self, bound, mask=True)

        Parameter ``bound`` (int):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.PCG32.seed(self, size=1, initstate=9600629759793949339, initseq=15726070495360670683)

        Parameter ``size`` (int):
            *no description available*

        Parameter ``initstate`` (drjit.llvm.ad.UInt64):
            *no description available*

        Parameter ``initseq`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.ParamFlags

    This list of flags is used to classify the different types of
    parameters exposed by the plugins.

    For instance, in the context of differentiable rendering, it is
    important to know which parameters can be differentiated, and which of
    those might introduce discontinuities in the Monte Carlo simulation.

    Members:

    .. py:data:: Differentiable

        Tracking gradients w.r.t. this parameter is allowed

    .. py:data:: NonDifferentiable

        Tracking gradients w.r.t. this parameter is not allowed

    .. py:data:: Discontinuous

        Tracking gradients w.r.t. this parameter will introduce
        discontinuities

    .. py:method:: __init__(self, value)

        Parameter ``value`` (int):
            *no description available*


    .. py:method:: mitsuba.ParamFlags.name
        :property:

.. py:class:: mitsuba.PhaseFunction

    Base class: :py:obj:`mitsuba.Object`

    .. py:method:: mitsuba.PhaseFunction.component_count(self, active=True)

        Number of components this phase function is comprised of.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.PhaseFunction.eval(self, ctx, mi, wo, active=True)

        Evaluates the phase function model

        The function returns the value (which equals the PDF) of the phase
        function in the query direction.

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

        Returns → drjit.llvm.ad.Float:
            The value of the phase function in direction wo

    .. py:method:: mitsuba.PhaseFunction.flags(overloaded)


        .. py:method:: flags(self, index, active=True)

            Flags for a specific component of this phase function.

            Parameter ``index`` (int):
                *no description available*

            Parameter ``active`` (drjit.llvm.ad.Bool):
                Mask to specify active lanes.

            Returns → int:
                *no description available*

        .. py:method:: flags(self, active=True)

            Flags for this phase function.

            Parameter ``active`` (drjit.llvm.ad.Bool):
                Mask to specify active lanes.

            Returns → int:
                *no description available*

    .. py:method:: mitsuba.PhaseFunction.id(self)

        Return a string identifier

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.PhaseFunction.max_projected_area(self)

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

        Returns → Tuple[:py:obj:`mitsuba.Vector3f`, drjit.llvm.ad.Float]:
            A sampled direction wo

.. py:class:: mitsuba.PhaseFunctionContext

    //! @}

    .. py:method:: mitsuba.PhaseFunctionContext.reverse(self)

        Reverse the direction of light transport in the record

        This updates the transport mode (radiance to importance and vice
        versa).

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionContext.sampler
        :property:

        Sampler object

.. py:class:: mitsuba.PhaseFunctionFlags

    This enumeration is used to classify phase functions into different
    types, i.e. into isotropic, anisotropic and microflake phase
    functions.

    This can be used to optimize implementations to for example have less
    overhead if the phase function is not a microflake phase function.

    Members:

    .. py:data:: Empty



    .. py:data:: Isotropic



    .. py:data:: Anisotropic



    .. py:data:: Microflake



    .. py:method:: __init__(self, value)

        Parameter ``value`` (int):
            *no description available*


    .. py:method:: mitsuba.PhaseFunctionFlags.name
        :property:

.. py:class:: mitsuba.PhaseFunctionPtr


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.PhaseFunction`):
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.PhaseFunctionPtr`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.component_count(self, active=True)

        Number of components this phase function is comprised of.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.PhaseFunction`:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.eq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.PhaseFunctionPtr`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.eval(self, ctx, mi, wo, active=True)

        Evaluates the phase function model

        The function returns the value (which equals the PDF) of the phase
        function in the query direction.

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

        Returns → drjit.llvm.ad.Float:
            The value of the phase function in direction wo

    .. py:method:: mitsuba.PhaseFunctionPtr.flags(self, active=True)

        Flags for this phase function.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.gather_(source, index, mask, permute=False)

        Parameter ``source`` (:py:obj:`mitsuba.PhaseFunctionPtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → :py:obj:`mitsuba.PhaseFunctionPtr`:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.max_projected_area(self)

        Return the maximum projected area of the microflake distribution

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.neq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.PhaseFunctionPtr`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
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

    .. py:method:: mitsuba.PhaseFunctionPtr.registry_get_max_()

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.registry_get_ptr_(arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.reinterpret_array_(arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → :py:obj:`mitsuba.PhaseFunctionPtr`:
            *no description available*

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

        Returns → Tuple[:py:obj:`mitsuba.Vector3f`, drjit.llvm.ad.Float]:
            A sampled direction wo

    .. py:method:: mitsuba.PhaseFunctionPtr.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (:py:obj:`mitsuba.PhaseFunctionPtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.select_(arg0, arg1, arg2)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.PhaseFunctionPtr`):
            *no description available*

        Parameter ``arg2`` (:py:obj:`mitsuba.PhaseFunctionPtr`):
            *no description available*

        Returns → :py:obj:`mitsuba.PhaseFunctionPtr`:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.zero_

        (arg0: int) -> :py:obj:`mitsuba.llvm_ad_rgb.PhaseFunctionPtr`

.. py:class:: mitsuba.PluginManager

    The object factory is responsible for loading plugin modules and
    instantiating object instances.

    Ordinarily, this class will be used by making repeated calls to the
    create_object() methods. The generated instances are then assembled
    into a final object graph, such as a scene. One such examples is the
    SceneHandler class, which parses an XML scene file by essentially
    translating the XML elements into calls to create_object().

    .. py:method:: mitsuba.PluginManager.create_object(self, arg0)

        Instantiate a plugin, verify its type, and return the newly created
        object instance.

        Parameter ``props``:
            A Properties instance containing all information required to find
            and construct the plugin.

        Parameter ``class_type``:
            Expected type of the instance. An exception will be thrown if it
            turns out not to derive from this class.

        Parameter ``arg0`` (mitsuba::Properties):
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

    .. py:method:: mitsuba.PluginManager.instance()

        Return the global plugin manager

        Returns → :py:obj:`mitsuba.PluginManager`:
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


    .. py:method:: __init__(self)

        Construct an uninitialized position sample

    .. py:method:: __init__(self, other)

        Copy constructor

        Parameter ``other`` (:py:obj:`mitsuba.PositionSample3f`):
            *no description available*

    .. py:method:: __init__(self, si)

        Create a position sampling record from a surface intersection

        This is useful to determine the hypothetical sampling density on a
        surface after hitting it using standard ray tracing. This happens for
        instance in path tracing with multiple importance sampling.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            *no description available*

    .. py:method:: mitsuba.PositionSample3f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.PositionSample3f`):
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


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.PreliminaryIntersection3f`):
            *no description available*

    .. py:method:: mitsuba.PreliminaryIntersection3f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.PreliminaryIntersection3f`):
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

    .. py:method:: mitsuba.PreliminaryIntersection3f.is_valid(self)

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

    .. py:method:: mitsuba.PreliminaryIntersection3f.zero_(self, arg0)

        This callback method is invoked by dr::zeros<>, and takes care of
        fields that deviate from the standard zero-initialization convention.
        In this particular class, the ``t`` field should be set to an infinite
        value to mark invalid intersection records.

        Parameter ``arg0`` (int):
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

    .. py:method:: mitsuba.ProjectiveCamera.far_clip(self)

        Return the far clip plane distance

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ProjectiveCamera.focus_distance(self)

        Return the distance to the focal plane

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ProjectiveCamera.near_clip(self)

        Return the near clip plane distance

        Returns → float:
            *no description available*

.. py:class:: mitsuba.Properties

    Associative parameter map for constructing subclasses of Object.

    Note that the Python bindings for this class do not implement the
    various type-dependent getters and setters. Instead, they are accessed
    just like a normal Python map, e.g:

    .. code-block:: c

        myProps = mitsuba.core.Properties("plugin_name")
        myProps["stringProperty"] = "hello"
        myProps["spectrumProperty"] = mitsuba.core.Spectrum(1.0)


    or using the ``get(key, default)`` method.


    .. py:method:: __init__(self)

        Construct an empty property container

    .. py:method:: __init__(self, arg0)

        Construct an empty property container with a specific plugin name

        Parameter ``arg0`` (str):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.Properties`):
            *no description available*

    .. py:class:: mitsuba.Properties.Type

        Members:

            Bool

            Long

            Float

            Array3f

            Transform

            Color

            String

            NamedReference

            Object

            Pointer

        .. py:method:: __init__(self, value)

            Parameter ``value`` (int):
                *no description available*


    .. py:method:: mitsuba.Properties.Type.name
        :property:

    .. py:method:: mitsuba.Properties.as_string(self, arg0)

        Return one of the parameters (converting it to a string if necessary)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Properties.copy_attribute(self, arg0, arg1, arg2)

        Copy a single attribute from another Properties object and potentially
        rename it

        Parameter ``arg0`` (:py:obj:`mitsuba.Properties`):
            *no description available*

        Parameter ``arg1`` (str):
            *no description available*

        Parameter ``arg2`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Properties.get(self, key, def_value=None)

        Return the value for the specified key it exists, otherwise return default value

        Parameter ``key`` (str):
            *no description available*

        Parameter ``def_value`` (object):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Properties.has_property(self, arg0)

        Verify if a value with the specified name exists

        Parameter ``arg0`` (str):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Properties.id(self)

        Returns a unique identifier associated with this instance (or an empty
        string)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Properties.mark_queried(self, arg0)

        Manually mark a certain property as queried

        Parameter ``arg0`` (str):
            *no description available*

        Returns → bool:
            ``True`` upon success

    .. py:method:: mitsuba.Properties.merge(self, arg0)

        Merge another properties record into the current one.

        Existing properties will be overwritten with the values from ``props``
        if they have the same name.

        Parameter ``arg0`` (:py:obj:`mitsuba.Properties`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Properties.named_references(self)

        Returns → List[Tuple[str, str]]:
            *no description available*

    .. py:method:: mitsuba.Properties.plugin_name(self)

        Get the associated plugin name

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Properties.property_names(self)

        Return an array containing the names of all stored properties

        Returns → List[str]:
            *no description available*

    .. py:method:: mitsuba.Properties.remove_property(self, arg0)

        Remove a property with the specified name

        Parameter ``arg0`` (str):
            *no description available*

        Returns → bool:
            ``True`` upon success

    .. py:method:: mitsuba.Properties.set_id(self, arg0)

        Set the unique identifier associated with this instance

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Properties.set_plugin_name(self, arg0)

        Set the associated plugin name

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Properties.string(self, arg0, arg1)

        Retrieve a string value (use default value if no entry exists)

        Parameter ``arg0`` (str):
            *no description available*

        Parameter ``arg1`` (str):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Properties.type(self, arg0)

        Returns the type of an existing property. If no property exists under
        that name, an error is logged and type ``void`` is returned.

        Parameter ``arg0`` (str):
            *no description available*

        Returns → mitsuba::Properties::Type:
            *no description available*

    .. py:method:: mitsuba.Properties.unqueried(self)

        Return the list of un-queried attributed

        Returns → List[str]:
            *no description available*

    .. py:method:: mitsuba.Properties.was_queried(self, arg0)

        Check if a certain property was queried

        Parameter ``arg0`` (str):
            *no description available*

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.Quaternion4f

    .. py:method:: mitsuba.Quaternion4f.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Quaternion4f.entry_ref_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Quaternion4f.set_entry_(self, arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → None:
            *no description available*

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


    .. py:method:: mitsuba.RadicalInverse.base(self, arg0)

        Returns the n-th prime base used by the sequence

        These prime numbers are used as bases in the radical inverse function
        implementation.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.RadicalInverse.bases(self)

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

    .. py:method:: mitsuba.RadicalInverse.inverse_permutation(self, arg0)

        Return the inverse permutation corresponding to the given prime number
        basis

        Parameter ``arg0`` (int):
            *no description available*

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.RadicalInverse.permutation(self, arg0)

        Return the permutation corresponding to the given prime number basis

        Parameter ``arg0`` (int):
            *no description available*

        Returns → numpy.ndarray[numpy.uint16]:
            *no description available*

    .. py:method:: mitsuba.RadicalInverse.scramble(self)

        Return the original scramble value

        Returns → int:
            *no description available*

.. py:class:: mitsuba.Ray2f

    Simple n-dimensional ray segment data structure

    Along with the ray origin and direction, this data structure
    additionally stores a maximum ray position ``maxt``, a time value
    ``time`` as well a the wavelength information associated with the ray.


    .. py:method:: __init__(self)

        Create an uninitialized ray

    .. py:method:: __init__(self, other)

        Copy constructor

        Parameter ``other`` (:py:obj:`mitsuba.Ray2f`):
            *no description available*

    .. py:method:: __init__(self, o, d, time=0.0, wavelengths=[])

        Construct a new ray (o, d) with time

        Parameter ``o`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``d`` (:py:obj:`mitsuba.Vector2f`):
            *no description available*

        Parameter ``time`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``wavelengths`` (:py:obj:`mitsuba.Color0f`):
            *no description available*

    .. py:method:: __init__(self, o, d, maxt, time, wavelengths)

        Construct a new ray (o, d) with bounds

        Parameter ``o`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``d`` (:py:obj:`mitsuba.Vector2f`):
            *no description available*

        Parameter ``maxt`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``time`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``wavelengths`` (:py:obj:`mitsuba.Color0f`):
            *no description available*

    .. py:method:: __init__(self, other, maxt)

        Copy a ray, but change the maxt value

        Parameter ``other`` (:py:obj:`mitsuba.Ray2f`):
            *no description available*

        Parameter ``maxt`` (drjit.llvm.ad.Float):
            *no description available*

    .. py:method:: mitsuba.Ray2f.__call__(self, t)

        Return the position of a point along the ray

        Parameter ``t`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → :py:obj:`mitsuba.Point2f`:
            *no description available*

    .. py:method:: mitsuba.Ray2f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Ray2f`):
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

.. py:class:: mitsuba.Ray3f

    Simple n-dimensional ray segment data structure

    Along with the ray origin and direction, this data structure
    additionally stores a maximum ray position ``maxt``, a time value
    ``time`` as well a the wavelength information associated with the ray.


    .. py:method:: __init__(self)

        Create an uninitialized ray

    .. py:method:: __init__(self, other)

        Copy constructor

        Parameter ``other`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

    .. py:method:: __init__(self, o, d, time=0.0, wavelengths=[])

        Construct a new ray (o, d) with time

        Parameter ``o`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``d`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Parameter ``time`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``wavelengths`` (:py:obj:`mitsuba.Color0f`):
            *no description available*

    .. py:method:: __init__(self, o, d, maxt, time, wavelengths)

        Construct a new ray (o, d) with bounds

        Parameter ``o`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``d`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Parameter ``maxt`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``time`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``wavelengths`` (:py:obj:`mitsuba.Color0f`):
            *no description available*

    .. py:method:: __init__(self, other, maxt)

        Copy a ray, but change the maxt value

        Parameter ``other`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Parameter ``maxt`` (drjit.llvm.ad.Float):
            *no description available*

    .. py:method:: mitsuba.Ray3f.__call__(self, t)

        Return the position of a point along the ray

        Parameter ``t`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → :py:obj:`mitsuba.Point3f`:
            *no description available*

    .. py:method:: mitsuba.Ray3f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Ray3f`):
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


    .. py:method:: __init__(self)

        Create an uninitialized ray

    .. py:method:: __init__(self, ray)

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

    .. py:method:: __init__(self, o, d, time=0.0, wavelengths=[])

        Initialize without differentials.

        Parameter ``o`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``d`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Parameter ``time`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``wavelengths`` (:py:obj:`mitsuba.Color0f`):
            *no description available*

    .. py:method:: mitsuba.RayDifferential3f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.RayDifferential3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.RayDifferential3f.scale_differential(self, amount)

        Parameter ``amount`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.RayFlags

    Members:

        Empty : No flags set

        Minimal : Compute position and geometric normal

        UV : Compute UV coordinates

        dPdUV : Compute position partials wrt. UV coordinates

        dNGdUV : Compute the geometric normal partials wrt. the UV coordinates

        dNSdUV : Compute the shading normal partials wrt. the UV coordinates

        ShadingFrame : Compute shading normal and shading frame

        FollowShape : Derivatives of the SurfaceInteraction fields follow shape's motion

        DetachShape : Derivatives of the SurfaceInteraction fields ignore shape's motion

        BoundaryTest : Compute the boundary-test used in reparameterized integrators

        All : //! Compound compute flags

        AllNonDifferentiable : Compute all fields of the surface interaction ignoring shape's motion

    .. py:method:: __init__(self, value)

        Parameter ``value`` (int):
            *no description available*


    .. py:method:: mitsuba.RayFlags.name
        :property:

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

    .. py:method:: mitsuba.ReconstructionFilter.border_size(self)

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

    .. py:method:: mitsuba.ReconstructionFilter.is_box_filter(self)

        Check whether this is a box filter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ReconstructionFilter.radius(self)

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

        Parameter ``rfilter`` (:py:obj:`mitsuba.ReconstructionFilter`):
            *no description available*


    .. py:method:: mitsuba.Resampler.boundary_condition(self)

        Return the boundary condition that should be used when looking up
        samples outside of the defined input domain

        Returns → :py:obj:`mitsuba.FilterBoundaryCondition`:
            *no description available*

    .. py:method:: mitsuba.Resampler.clamp(self)

        Returns the range to which resampled values will be clamped

        The default is -infinity to infinity (i.e. no clamping is used)

        Returns → Tuple[float, float]:
            *no description available*

    .. py:method:: mitsuba.Resampler.resample(self, source, source_stride, target, target_stride, channels)

        Resample a multi-channel array and clamp the results to a specified
        valid range

        Parameter ``source`` (numpy.ndarray[numpy.float32]):
            Source array of samples

        Parameter ``target`` (numpy.ndarray[numpy.float32]):
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

    .. py:method:: mitsuba.Resampler.set_boundary_condition(self, arg0)

        Set the boundary condition that should be used when looking up samples
        outside of the defined input domain

        The default is FilterBoundaryCondition::Clamp

        Parameter ``arg0`` (:py:obj:`mitsuba.FilterBoundaryCondition`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Resampler.set_clamp(self, arg0)

        If specified, resampled values will be clamped to the given range

        Parameter ``arg0`` (Tuple[float, float]):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Resampler.source_resolution(self)

        Return the reconstruction filter's source resolution

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Resampler.taps(self)

        Return the number of taps used by the reconstruction filter

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Resampler.target_resolution(self)

        Return the reconstruction filter's target resolution

        Returns → int:
            *no description available*

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


    .. py:method:: mitsuba.Sampler.advance(self)

        Advance to the next sample.

        A subsequent call to ``next_1d`` or ``next_2d`` will access the first
        1D or 2D components of this sample.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Sampler.clone(self)

        Create a clone of this sampler.

        Subsequent calls to the cloned sampler will produce the same random
        numbers as the original sampler.

        Remark:
            This method relies on the overload of the copy constructor.

        May throw an exception if not supported.

        Returns → :py:obj:`mitsuba.Sampler`:
            *no description available*

    .. py:method:: mitsuba.Sampler.fork(self)

        Create a fork of this sampler.

        A subsequent call to ``seed`` is necessary to properly initialize the
        internal state of the sampler.

        May throw an exception if not supported.

        Returns → :py:obj:`mitsuba.Sampler`:
            *no description available*

    .. py:method:: mitsuba.Sampler.loop_put(self, loop)

        Register internal state of this sampler with a symbolic loop

        Parameter ``loop`` (drjit.llvm.ad.LoopBase):
            *no description available*

        Returns → None:
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

    .. py:method:: mitsuba.Sampler.sample_count(self)

        Return the number of samples per pixel

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Sampler.schedule_state(self)

        dr::schedule() variables that represent the internal sampler state

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Sampler.seed(self, seed, wavefront_size=4294967295)

        Deterministically seed the underlying RNG, if applicable.

        In the context of wavefront ray tracing & dynamic arrays, this
        function must be called with ``wavefront_size`` matching the size of
        the wavefront.

        Parameter ``seed`` (int):
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

    .. py:method:: mitsuba.Sampler.wavefront_size(self)

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

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Properties`):
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

        Parameter ``medium`` (:py:obj:`mitsuba.Medium`):
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

        Returns → Tuple[:py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Bool, List[drjit.llvm.ad.Float]]:
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


    .. py:method:: __init__(self)

        Create a new invalid bounding box

        Initializes the components of the minimum and maximum position to
        :math:`\infty` and :math:`-\infty`, respectively.

    .. py:method:: __init__(self, p)

        Create a collapsed bounding box from a single point

        Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint2f`):
            *no description available*

    .. py:method:: __init__(self, min, max)

        Create a bounding box from two positions

        Parameter ``min`` (:py:obj:`mitsuba.ScalarPoint2f`):
            *no description available*

        Parameter ``max`` (:py:obj:`mitsuba.ScalarPoint2f`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarBoundingBox2f`):
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.center(self)

        Return the center point

        Returns → :py:obj:`mitsuba.ScalarPoint2f`:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.clip(self, arg0)

        Clip this bounding box to another bounding box

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarBoundingBox2f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.collapsed(self)

        Check whether this bounding box has collapsed to a point, line, or
        plane

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.contains(overloaded)


        .. py:method:: contains(self, p, strict=False)

            Check whether a point lies *on* or *inside* the bounding box

            Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint2f`):
                The point to be tested

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

        .. py:method:: contains(self, bbox, strict=False)

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

            Parameter ``bbox`` (:py:obj:`mitsuba.ScalarBoundingBox2f`):
                *no description available*

            Parameter ``strict`` (bool):
                *no description available*

            Returns → bool:
                *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.corner(self, arg0)

        Return the position of a bounding box corner

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarPoint2f`:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.distance(overloaded)


        .. py:method:: distance(self, arg0)

            Calculate the shortest distance between the axis-aligned bounding box
            and the point ``p``.

            Parameter ``arg0`` (:py:obj:`mitsuba.ScalarPoint2f`):
                *no description available*

            Returns → float:
                *no description available*

        .. py:method:: distance(self, arg0)

            Calculate the shortest distance between the axis-aligned bounding box
            and ``bbox``.

            Parameter ``arg0`` (:py:obj:`mitsuba.ScalarBoundingBox2f`):
                *no description available*

            Returns → float:
                *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.expand(overloaded)


        .. py:method:: expand(self, arg0)

            Expand the bounding box to contain another point

            Parameter ``arg0`` (:py:obj:`mitsuba.ScalarPoint2f`):
                *no description available*

        .. py:method:: expand(self, arg0)

            Expand the bounding box to contain another bounding box

            Parameter ``arg0`` (:py:obj:`mitsuba.ScalarBoundingBox2f`):
                *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.extents(self)

        Calculate the bounding box extents

        Returns → :py:obj:`mitsuba.ScalarVector2f`:
            ``max - min``

    .. py:method:: mitsuba.ScalarBoundingBox2f.major_axis(self)

        Return the dimension index with the index associated side length

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.merge(arg0, arg1)

        Merge two bounding boxes

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarBoundingBox2f`):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.ScalarBoundingBox2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarBoundingBox2f`:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.minor_axis(self)

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

    .. py:method:: mitsuba.ScalarBoundingBox2f.reset(self)

        Mark the bounding box as invalid.

        This operation sets the components of the minimum and maximum position
        to :math:`\infty` and :math:`-\infty`, respectively.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.squared_distance(overloaded)


        .. py:method:: squared_distance(self, arg0)

            Calculate the shortest squared distance between the axis-aligned
            bounding box and the point ``p``.

            Parameter ``arg0`` (:py:obj:`mitsuba.ScalarPoint2f`):
                *no description available*

            Returns → float:
                *no description available*

        .. py:method:: squared_distance(self, arg0)

            Calculate the shortest squared distance between the axis-aligned
            bounding box and ``bbox``.

            Parameter ``arg0`` (:py:obj:`mitsuba.ScalarBoundingBox2f`):
                *no description available*

            Returns → float:
                *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.surface_area(self)

        Calculate the 2-dimensional surface area of a 3D bounding box

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.valid(self)

        Check whether this is a valid bounding box

        A bounding box ``bbox`` is considered to be valid when

        .. code-block:: c

            bbox.min[i] <= bbox.max[i]


        holds for each component ``i``.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.volume(self)

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


    .. py:method:: __init__(self)

        Create a new invalid bounding box

        Initializes the components of the minimum and maximum position to
        :math:`\infty` and :math:`-\infty`, respectively.

    .. py:method:: __init__(self, p)

        Create a collapsed bounding box from a single point

        Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

    .. py:method:: __init__(self, min, max)

        Create a bounding box from two positions

        Parameter ``min`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

        Parameter ``max`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarBoundingBox3f`):
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.bounding_sphere(self)

        Create a bounding sphere, which contains the axis-aligned box

        Returns → :py:obj:`mitsuba.BoundingSphere`:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.center(self)

        Return the center point

        Returns → :py:obj:`mitsuba.ScalarPoint3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.clip(self, arg0)

        Clip this bounding box to another bounding box

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarBoundingBox3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.collapsed(self)

        Check whether this bounding box has collapsed to a point, line, or
        plane

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.contains(overloaded)


        .. py:method:: contains(self, p, strict=False)

            Check whether a point lies *on* or *inside* the bounding box

            Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint3f`):
                The point to be tested

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

        .. py:method:: contains(self, bbox, strict=False)

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

            Parameter ``bbox`` (:py:obj:`mitsuba.ScalarBoundingBox3f`):
                *no description available*

            Parameter ``strict`` (bool):
                *no description available*

            Returns → bool:
                *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.corner(self, arg0)

        Return the position of a bounding box corner

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarPoint3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.distance(overloaded)


        .. py:method:: distance(self, arg0)

            Calculate the shortest distance between the axis-aligned bounding box
            and the point ``p``.

            Parameter ``arg0`` (:py:obj:`mitsuba.ScalarPoint3f`):
                *no description available*

            Returns → float:
                *no description available*

        .. py:method:: distance(self, arg0)

            Calculate the shortest distance between the axis-aligned bounding box
            and ``bbox``.

            Parameter ``arg0`` (:py:obj:`mitsuba.ScalarBoundingBox3f`):
                *no description available*

            Returns → float:
                *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.expand(overloaded)


        .. py:method:: expand(self, arg0)

            Expand the bounding box to contain another point

            Parameter ``arg0`` (:py:obj:`mitsuba.ScalarPoint3f`):
                *no description available*

        .. py:method:: expand(self, arg0)

            Expand the bounding box to contain another bounding box

            Parameter ``arg0`` (:py:obj:`mitsuba.ScalarBoundingBox3f`):
                *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.extents(self)

        Calculate the bounding box extents

        Returns → :py:obj:`mitsuba.ScalarVector3f`:
            ``max - min``

    .. py:method:: mitsuba.ScalarBoundingBox3f.major_axis(self)

        Return the dimension index with the index associated side length

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.merge(arg0, arg1)

        Merge two bounding boxes

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarBoundingBox3f`):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.ScalarBoundingBox3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarBoundingBox3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.minor_axis(self)

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

        Returns → Tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.reset(self)

        Mark the bounding box as invalid.

        This operation sets the components of the minimum and maximum position
        to :math:`\infty` and :math:`-\infty`, respectively.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.squared_distance(overloaded)


        .. py:method:: squared_distance(self, arg0)

            Calculate the shortest squared distance between the axis-aligned
            bounding box and the point ``p``.

            Parameter ``arg0`` (:py:obj:`mitsuba.ScalarPoint3f`):
                *no description available*

            Returns → float:
                *no description available*

        .. py:method:: squared_distance(self, arg0)

            Calculate the shortest squared distance between the axis-aligned
            bounding box and ``bbox``.

            Parameter ``arg0`` (:py:obj:`mitsuba.ScalarBoundingBox3f`):
                *no description available*

            Returns → float:
                *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.surface_area(self)

        Calculate the 2-dimensional surface area of a 3D bounding box

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.valid(self)

        Check whether this is a valid bounding box

        A bounding box ``bbox`` is considered to be valid when

        .. code-block:: c

            bbox.min[i] <= bbox.max[i]


        holds for each component ``i``.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.volume(self)

        Calculate the n-dimensional volume of the bounding box

        Returns → float:
            *no description available*

.. py:class:: mitsuba.ScalarBoundingSphere3f

    Generic n-dimensional bounding sphere data structure


    .. py:method:: __init__(self)

        Construct bounding sphere(s) at the origin having radius zero

    .. py:method:: __init__(self, arg0, arg1)

        Create bounding sphere(s) from given center point(s) with given
        size(s)

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

        Parameter ``arg1`` (float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarBoundingSphere3f`):
            *no description available*

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

    .. py:method:: mitsuba.ScalarBoundingSphere3f.empty(self)

        Return whether this bounding sphere has a radius of zero or less.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingSphere3f.expand(self, arg0)

        Expand the bounding sphere radius to contain another point.

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingSphere3f.ray_intersect(self, ray)

        Check if a ray intersects a bounding box

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Returns → Tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
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


    .. py:method:: __init__(self)

        Initialize with the identity matrix

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarTransform3d`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (numpy.ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Initialize the transformation from the given matrix (and compute its
        inverse transpose)

        Parameter ``arg0`` (drjit.scalar.Matrix3f64):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1)

        Initialize from a matrix and its inverse transpose

        Parameter ``arg0`` (drjit.scalar.Matrix3f64):
            *no description available*

        Parameter ``arg1`` (drjit.scalar.Matrix3f64):
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarTransform3d`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.has_scale(overloaded)


        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → bool:
                *no description available*

        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → bool:
                *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.inverse(self)

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.ScalarTransform3d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.rotate(angle)

        Create a rotation transformation in 2D. The angle is specified in
        degrees

        Parameter ``angle`` (float):
            *no description available*

        Returns → ChainTransform<double, 3>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.scale(v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint2d`):
            *no description available*

        Returns → ChainTransform<double, 3>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.transform_affine(overloaded)


        .. py:method:: transform_affine(self, p)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint2d`):
                *no description available*

            Returns → :py:obj:`mitsuba.ScalarPoint2d`:
                *no description available*

        .. py:method:: transform_affine(self, v)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``v`` (:py:obj:`mitsuba.ScalarVector2d`):
                *no description available*

            Returns → :py:obj:`mitsuba.ScalarVector2d`:
                *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.translate(v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint2d`):
            *no description available*

        Returns → ChainTransform<double, 3>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.translation(self)

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


    .. py:method:: __init__(self)

        Initialize with the identity matrix

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarTransform3f`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (numpy.ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Initialize the transformation from the given matrix (and compute its
        inverse transpose)

        Parameter ``arg0`` (drjit.scalar.Matrix3f):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1)

        Initialize from a matrix and its inverse transpose

        Parameter ``arg0`` (drjit.scalar.Matrix3f):
            *no description available*

        Parameter ``arg1`` (drjit.scalar.Matrix3f):
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarTransform3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.has_scale(overloaded)


        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → bool:
                *no description available*

        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → bool:
                *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.inverse(self)

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.ScalarTransform3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.rotate(angle)

        Create a rotation transformation in 2D. The angle is specified in
        degrees

        Parameter ``angle`` (float):
            *no description available*

        Returns → ChainTransform<float, 3>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.scale(v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint2f`):
            *no description available*

        Returns → ChainTransform<float, 3>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.transform_affine(overloaded)


        .. py:method:: transform_affine(self, p)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint2f`):
                *no description available*

            Returns → :py:obj:`mitsuba.ScalarPoint2f`:
                *no description available*

        .. py:method:: transform_affine(self, v)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``v`` (:py:obj:`mitsuba.ScalarVector2f`):
                *no description available*

            Returns → :py:obj:`mitsuba.ScalarVector2f`:
                *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.translate(v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint2f`):
            *no description available*

        Returns → ChainTransform<float, 3>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.translation(self)

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


    .. py:method:: __init__(self)

        Initialize with the identity matrix

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarTransform4d`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (numpy.ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Initialize the transformation from the given matrix (and compute its
        inverse transpose)

        Parameter ``arg0`` (drjit.scalar.Matrix4f64):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1)

        Initialize from a matrix and its inverse transpose

        Parameter ``arg0`` (drjit.scalar.Matrix4f64):
            *no description available*

        Parameter ``arg1`` (drjit.scalar.Matrix4f64):
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarTransform4d`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.extract(self)

        Extract a lower-dimensional submatrix

        Returns → :py:obj:`mitsuba.ScalarTransform3d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.from_frame(frame)

        Creates a transformation that converts from 'frame' to the standard
        basis

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → ChainTransform<double, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.has_scale(overloaded)


        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → bool:
                *no description available*

        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → bool:
                *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.inverse(self)

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.ScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.look_at(origin, target, up)

        Create a look-at camera transformation

        Parameter ``origin`` (:py:obj:`mitsuba.ScalarPoint3d`):
            Camera position

        Parameter ``target`` (:py:obj:`mitsuba.ScalarPoint3d`):
            Target vector

        Parameter ``up`` (:py:obj:`mitsuba.ScalarPoint3d`):
            Up vector

        Returns → ChainTransform<double, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.orthographic(near, far)

        Create an orthographic transformation, which maps Z to [0,1] and
        leaves the X and Y coordinates untouched.

        Parameter ``near`` (float):
            Near clipping plane

        Parameter ``far`` (float):
            Far clipping plane

        Returns → ChainTransform<double, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.perspective(fov, near, far)

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

        Returns → ChainTransform<double, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.rotate(axis, angle)

        Create a rotation transformation around an arbitrary axis in 3D. The
        angle is specified in degrees

        Parameter ``axis`` (:py:obj:`mitsuba.ScalarPoint3d`):
            *no description available*

        Parameter ``angle`` (float):
            *no description available*

        Returns → ChainTransform<double, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.scale(v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint3d`):
            *no description available*

        Returns → ChainTransform<double, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.to_frame(frame)

        Creates a transformation that converts from the standard basis to
        'frame'

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → ChainTransform<double, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.transform_affine(overloaded)


        .. py:method:: transform_affine(self, p)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint3d`):
                *no description available*

            Returns → :py:obj:`mitsuba.ScalarPoint3d`:
                *no description available*

        .. py:method:: transform_affine(self, ray)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``ray`` (:py:obj:`mitsuba.Ray`):
                *no description available*

            Returns → :py:obj:`mitsuba.Ray`:
                *no description available*

        .. py:method:: transform_affine(self, v)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``v`` (:py:obj:`mitsuba.ScalarVector3d`):
                *no description available*

            Returns → :py:obj:`mitsuba.ScalarVector3d`:
                *no description available*

        .. py:method:: transform_affine(self, n)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``n`` (:py:obj:`mitsuba.ScalarNormal3d`):
                *no description available*

            Returns → :py:obj:`mitsuba.ScalarNormal3d`:
                *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.translate(v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint3d`):
            *no description available*

        Returns → ChainTransform<double, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.translation(self)

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


    .. py:method:: __init__(self)

        Initialize with the identity matrix

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarTransform4f`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (numpy.ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Initialize the transformation from the given matrix (and compute its
        inverse transpose)

        Parameter ``arg0`` (drjit.scalar.Matrix4f):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1)

        Initialize from a matrix and its inverse transpose

        Parameter ``arg0`` (drjit.scalar.Matrix4f):
            *no description available*

        Parameter ``arg1`` (drjit.scalar.Matrix4f):
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.ScalarTransform4f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.extract(self)

        Extract a lower-dimensional submatrix

        Returns → :py:obj:`mitsuba.ScalarTransform3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.from_frame(frame)

        Creates a transformation that converts from 'frame' to the standard
        basis

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → ChainTransform<float, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.has_scale(overloaded)


        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → bool:
                *no description available*

        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → bool:
                *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.inverse(self)

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.ScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.look_at(origin, target, up)

        Create a look-at camera transformation

        Parameter ``origin`` (:py:obj:`mitsuba.ScalarPoint3f`):
            Camera position

        Parameter ``target`` (:py:obj:`mitsuba.ScalarPoint3f`):
            Target vector

        Parameter ``up`` (:py:obj:`mitsuba.ScalarPoint3f`):
            Up vector

        Returns → ChainTransform<float, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.orthographic(near, far)

        Create an orthographic transformation, which maps Z to [0,1] and
        leaves the X and Y coordinates untouched.

        Parameter ``near`` (float):
            Near clipping plane

        Parameter ``far`` (float):
            Far clipping plane

        Returns → ChainTransform<float, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.perspective(fov, near, far)

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

        Returns → ChainTransform<float, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.rotate(axis, angle)

        Create a rotation transformation around an arbitrary axis in 3D. The
        angle is specified in degrees

        Parameter ``axis`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

        Parameter ``angle`` (float):
            *no description available*

        Returns → ChainTransform<float, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.scale(v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

        Returns → ChainTransform<float, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.to_frame(frame)

        Creates a transformation that converts from the standard basis to
        'frame'

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → ChainTransform<float, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.transform_affine(overloaded)


        .. py:method:: transform_affine(self, p)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint3f`):
                *no description available*

            Returns → :py:obj:`mitsuba.ScalarPoint3f`:
                *no description available*

        .. py:method:: transform_affine(self, ray)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``ray`` (:py:obj:`mitsuba.Ray`):
                *no description available*

            Returns → :py:obj:`mitsuba.Ray`:
                *no description available*

        .. py:method:: transform_affine(self, v)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``v`` (:py:obj:`mitsuba.ScalarVector3f`):
                *no description available*

            Returns → :py:obj:`mitsuba.ScalarVector3f`:
                *no description available*

        .. py:method:: transform_affine(self, n)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``n`` (:py:obj:`mitsuba.ScalarNormal3f`):
                *no description available*

            Returns → :py:obj:`mitsuba.ScalarNormal3f`:
                *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.translate(v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

        Returns → ChainTransform<float, 4>:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.translation(self)

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

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Properties`):
            *no description available*


    .. py:method:: mitsuba.Scene.bbox(self)

        Return a bounding box surrounding the scene

        Returns → :py:obj:`mitsuba.ScalarBoundingBox3f`:
            *no description available*

    .. py:method:: mitsuba.Scene.emitters(self)

        Return the list of emitters

        Returns → List[:py:obj:`mitsuba.Emitter`]:
            *no description available*

    .. py:method:: mitsuba.Scene.emitters_dr(self)

        Return the list of emitters as an Dr.Jit array

        Returns → drjit::DiffArray<drjit::LLVMArray<:py:obj:`mitsuba.Emitter` const*>>:
            *no description available*

    .. py:method:: mitsuba.Scene.environment(self)

        Return the environment emitter (if any)

        Returns → :py:obj:`mitsuba.Emitter`:
            *no description available*

    .. py:method:: mitsuba.Scene.eval_emitter_direction(self, ref, active=True)

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
        Alternatively (or additionally), it may be necessary to apply a
        spherical reparameterization to ``ds.d`` to handle visibility-induced
        discontinuities during differentiation. Both steps require re-
        evaluating the contribution of the emitter while tracking derivative
        information through the calculation.

        In contrast to pdf_emitter_direction(), evaluating this function can
        yield a nonzero result in the case of emission profiles containing a
        Dirac delta term (e.g. point or directional lights).

        Parameter ``ref`` (:py:obj:`mitsuba.Interaction`):
            A 3D reference location within the scene, which may influence the
            sampling process.

        Parameter ``ds``:
            A direction sampling record, which specifies the query location.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The incident radiance and discrete or solid angle density of the
            sample.

    .. py:method:: mitsuba.Scene.integrator(self)

        Return the scene's integrator

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Scene.pdf_emitter(self, index, active=True)

        Evaluate the discrete probability of the sample_emitter() technique
        for the given a emitter index.

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Scene.pdf_emitter_direction(self, ref, active=True)

        Evaluate the PDF of direct illumination sampling

        This function evaluates the probability density (per unit solid angle)
        of the sampling technique implemented by the sample_emitter_direct()
        function. The returned probability will always be zero when the
        emission profile contains a Dirac delta term (e.g. point or
        directional emitters/sensors).

        Parameter ``ref`` (:py:obj:`mitsuba.Interaction`):
            A 3D reference location within the scene, which may influence the
            sampling process.

        Parameter ``ds``:
            A direction sampling record, which specifies the query location.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The solid angle density of the sample

    .. py:method:: mitsuba.Scene.ray_intersect(overloaded)


        .. py:method:: ray_intersect(self, ray, active=True)

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

            Returns → :py:obj:`mitsuba.SurfaceInteraction`:
                A detailed surface interaction record. Its ``is_valid()`` method
                should be queried to check if an intersection was actually found.

            Parameter ``active`` (drjit.llvm.ad.Bool):
                Mask to specify active lanes.

        .. py:method:: ray_intersect(self, ray, ray_flags, coherent, active=True)

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

            Parameter ``ray_flags`` (int):
                An integer combining flag bits from RayFlags (merged using binary
                or).

            Parameter ``coherent`` (drjit.llvm.ad.Bool):
                Setting this flag to ``True`` can noticeably improve performance
                when ``ray`` contains a coherent set of rays (e.g. primary camera
                rays), and when using ``llvm_*`` variants of the renderer along
                with Embree. It has no effect in scalar or CUDA/OptiX variants.

            Returns → :py:obj:`mitsuba.SurfaceInteraction`:
                A detailed surface interaction record. Its ``is_valid()`` method
                should be queried to check if an intersection was actually found.

            Parameter ``active`` (drjit.llvm.ad.Bool):
                Mask to specify active lanes.

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

        Returns → :py:obj:`mitsuba.PreliminaryIntersection`:
            A preliminary surface interaction record. Its ``is_valid()``
            method should be queried to check if an intersection was actually
            found.

    .. py:method:: mitsuba.Scene.ray_test(overloaded)


        .. py:method:: ray_test(self, ray, active=True)

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

            Returns → drjit.llvm.ad.Bool:
                ``True`` if an intersection was found

            Parameter ``active`` (drjit.llvm.ad.Bool):
                Mask to specify active lanes.

        .. py:method:: ray_test(self, ray, coherent, active=True)

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

            Parameter ``coherent`` (drjit.llvm.ad.Bool):
                Setting this flag to ``True`` can noticeably improve performance
                when ``ray`` contains a coherent set of rays (e.g. primary camera
                rays), and when using ``llvm_*`` variants of the renderer along
                with Embree. It has no effect in scalar or CUDA/OptiX variants.

            Returns → drjit.llvm.ad.Bool:
                ``True`` if an intersection was found

            Parameter ``active`` (drjit.llvm.ad.Bool):
                Mask to specify active lanes.

    .. py:method:: mitsuba.Scene.sample_emitter(self, sample, active=True)

        Sample one emitter in the scene and rescale the input sample for
        reuse.

        Currently, the sampling scheme implemented by the Scene class is very
        simplistic (uniform).

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A uniformly distributed number in [0, 1).

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[drjit.llvm.ad.UInt, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
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

        Parameter ``ref`` (:py:obj:`mitsuba.Interaction`):
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

        Returns → Tuple[:py:obj:`mitsuba.DirectionSample`, :py:obj:`mitsuba.Color3f`]:
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

        Returns → Tuple[:py:obj:`mitsuba.Ray3f`, :py:obj:`mitsuba.Color3f`, drjit::DiffArray<drjit::LLVMArray<:py:obj:`mitsuba.Emitter` const*>>]:
            A tuple ``(ray, weight, emitter, radiance)``, where

        * ``ray`` is the sampled ray (e.g. starting on the surface of an area
        emitter)

        * ``weight`` returns the emitted radiance divided by the spatio-
        directional sampling density

        * ``emitter`` is a pointer specifying the sampled emitter

    .. py:method:: mitsuba.Scene.sensors(self)

        Return the list of sensors

        Returns → list:
            *no description available*

    .. py:method:: mitsuba.Scene.shapes(self)

        Return the list of shapes

        Returns → list:
            *no description available*

    .. py:method:: mitsuba.Scene.shapes_dr(self)

        Return the list of shapes as an Dr.Jit array

        Returns → drjit::DiffArray<drjit::LLVMArray<:py:obj:`mitsuba.Shape` const*>>:
            *no description available*

    .. py:method:: mitsuba.Scene.shapes_grad_enabled(self)

        Specifies whether any of the scene's shape parameters have tracking
        enabled

        Knowing this is important in the context of differentiable rendering:
        intersections (e.g. provided by OptiX or Embree) must then be re-
        computed differentiably within Dr.Jit to correctly track gradient
        information. Furthermore, differentiable geometry introduces bias
        through visibility-induced discontinuities, and reparameterizations
        (Loubet et al., SIGGRAPH 2019) are needed to avoid this bias.

        Returns → bool:
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

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.ThreadEnvironment`):
            *no description available*


.. py:class:: mitsuba.Sensor

    Base class: :py:obj:`mitsuba.Endpoint`

    .. py:method:: mitsuba.Sensor.film(self)

        Return the Film instance associated with this sensor

        Returns → :py:obj:`mitsuba.Film`:
            *no description available*

    .. py:method:: mitsuba.Sensor.needs_aperture_sample(self)

        Does the sampling technique require a sample for the aperture
        position?

        Returns → bool:
            *no description available*

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

        Returns → Tuple[:py:obj:`mitsuba.RayDifferential3f`, :py:obj:`mitsuba.Color3f`]:
            The sampled ray differential and (potentially spectrally varying)
            importance weights. The latter account for the difference between
            the sensor profile and the actual used sampling density function.

    .. py:method:: mitsuba.Sensor.sampler(self)

        Return the sensor's sample generator

        This is the *root* sampler, which will later be forked a number of
        times to provide each participating worker thread with its own
        instance (see Scene::sampler()). Therefore, this sampler should never
        be used for anything except creating forks.

        Returns → :py:obj:`mitsuba.Sampler`:
            *no description available*

    .. py:method:: mitsuba.Sensor.shutter_open(self)

        Return the time value of the shutter opening event

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.Sensor.shutter_open_time(self)

        Return the length, for which the shutter remains open

        Returns → float:
            *no description available*

.. py:class:: mitsuba.SensorPtr


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Sensor`):
            *no description available*

    .. py:method:: mitsuba.SensorPtr.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.SensorPtr`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.SensorPtr.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.Sensor`:
            *no description available*

    .. py:method:: mitsuba.SensorPtr.eq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.SensorPtr`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

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
        without also differentiating the sampling technique. Alternatively (or
        additionally), it may be necessary to apply a spherical
        reparameterization to ``ds.d`` to handle visibility-induced
        discontinuities during differentiation. Both steps require re-
        evaluating the contribution of the emitter while tracking derivative
        information through the calculation.

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

    .. py:method:: mitsuba.SensorPtr.gather_(source, index, mask, permute=False)

        Parameter ``source`` (:py:obj:`mitsuba.SensorPtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → :py:obj:`mitsuba.SensorPtr`:
            *no description available*

    .. py:method:: mitsuba.SensorPtr.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.SensorPtr.neq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.SensorPtr`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
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

    .. py:method:: mitsuba.SensorPtr.registry_get_max_()

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.SensorPtr.registry_get_ptr_(arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.SensorPtr.reinterpret_array_(arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → :py:obj:`mitsuba.SensorPtr`:
            *no description available*

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

        Returns → Tuple[:py:obj:`mitsuba.DirectionSample3f`, :py:obj:`mitsuba.Color3f`]:
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

        Returns → Tuple[:py:obj:`mitsuba.PositionSample3f`, drjit.llvm.ad.Float]:
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

        Returns → Tuple[:py:obj:`mitsuba.Ray3f`, :py:obj:`mitsuba.Color3f`]:
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

        Returns → Tuple[:py:obj:`mitsuba.RayDifferential3f`, :py:obj:`mitsuba.Color3f`]:
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

        Returns → Tuple[:py:obj:`mitsuba.Color0f`, :py:obj:`mitsuba.Color3f`]:
            The set of sampled wavelengths and (potentially spectrally
            varying) importance weights. The latter account for the difference
            between the profile and the actual used sampling density function.
            In the case of emitters, the weight will include the emitted
            radiance.

    .. py:method:: mitsuba.SensorPtr.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (:py:obj:`mitsuba.SensorPtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.SensorPtr.select_(arg0, arg1, arg2)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.SensorPtr`):
            *no description available*

        Parameter ``arg2`` (:py:obj:`mitsuba.SensorPtr`):
            *no description available*

        Returns → :py:obj:`mitsuba.SensorPtr`:
            *no description available*

    .. py:method:: mitsuba.SensorPtr.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.SensorPtr.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.SensorPtr.shape(self)

        Return the shape, to which the emitter is currently attached

        Returns → :py:obj:`mitsuba.ShapePtr`:
            *no description available*

    .. py:method:: mitsuba.SensorPtr.zero_

        (arg0: int) -> :py:obj:`mitsuba.llvm_ad_rgb.SensorPtr`

.. py:class:: mitsuba.Shape

    Base class: :py:obj:`mitsuba.Object`

    Base class of all geometric shapes in Mitsuba

    This class provides core functionality for sampling positions on
    surfaces, computing ray intersections, and bounding shapes within ray
    intersection acceleration data structures.

    .. py:method:: mitsuba.Shape.bbox(overloaded)


        .. py:method:: bbox(self)

            Return an axis aligned box that bounds all shape primitives (including
            any transformations that may have been applied to them)

            Returns → :py:obj:`mitsuba.ScalarBoundingBox3f`:
                *no description available*

        .. py:method:: bbox(self, index)

            Return an axis aligned box that bounds a single shape primitive
            (including any transformations that may have been applied to it)

            Remark:
                The default implementation simply calls bbox()

            Parameter ``index`` (int):
                *no description available*

            Returns → :py:obj:`mitsuba.ScalarBoundingBox3f`:
                *no description available*

        .. py:method:: bbox(self, index, clip)

            Return an axis aligned box that bounds a single shape primitive after
            it has been clipped to another bounding box.

            This is extremely important to construct high-quality kd-trees. The
            default implementation just takes the bounding box returned by
            bbox(ScalarIndex index) and clips it to *clip*.

            Parameter ``index`` (int):
                *no description available*

            Parameter ``clip`` (:py:obj:`mitsuba.ScalarBoundingBox3f`):
                *no description available*

            Returns → :py:obj:`mitsuba.ScalarBoundingBox3f`:
                *no description available*

    .. py:method:: mitsuba.Shape.bsdf(self)

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

        Parameter ``pi`` (:py:obj:`mitsuba.PreliminaryIntersection`):
            Data structure carrying information about the ray intersection

        Parameter ``ray_flags`` (int):
            Flags specifying which information should be computed

        Parameter ``recursion_depth``:
            Integer specifying the recursion depth for nested virtual function
            call to this method (e.g. used for instancing).

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SurfaceInteraction`:
            A data structure containing the detailed information

    .. py:method:: mitsuba.Shape.effective_primitive_count(self)

        Return the number of primitives (triangles, hairs, ..) contributed to
        the scene by this shape

        Includes instanced geometry. The default implementation simply returns
        the same value as primitive_count().

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Shape.emitter(self)

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

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction`):
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

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction`):
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

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction`):
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

        Returns → :py:obj:`mitsuba.SurfaceInteraction`:
            *no description available*

    .. py:method:: mitsuba.Shape.exterior_medium(self)

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

    .. py:method:: mitsuba.Shape.id(self)

        Return a string identifier

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Shape.interior_medium(self)

        Return the medium that lies on the interior of this shape

        Returns → :py:obj:`mitsuba.Medium`:
            *no description available*

    .. py:method:: mitsuba.Shape.is_emitter(self)

        Is this shape also an area emitter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Shape.is_medium_transition(self)

        Does the surface of this shape mark a medium transition?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Shape.is_mesh(self)

        Is this shape a triangle mesh?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Shape.is_sensor(self)

        Is this shape also an area sensor?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Shape.parameters_grad_enabled(self)

        Return whether any shape's parameters require gradients (default
        return false)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Shape.pdf_direction(self, it, active=True)

        Query the probability density of sample_direction()

        Parameter ``it`` (:py:obj:`mitsuba.Interaction`):
            A reference position somewhere within the scene.

        Parameter ``ps``:
            A position record describing the sample in question

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The probability density per unit solid angle

    .. py:method:: mitsuba.Shape.pdf_position(self, ps, active=True)

        Query the probability density of sample_position() for a particular
        point on the surface.

        Parameter ``ps`` (:py:obj:`mitsuba.PositionSample`):
            A position record describing the sample in question

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The probability density per unit area

    .. py:method:: mitsuba.Shape.primitive_count(self)

        Returns the number of sub-primitives that make up this shape

        Remark:
            The default implementation simply returns ``1``

        Returns → int:
            *no description available*

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

        Returns → :py:obj:`mitsuba.SurfaceInteraction`:
            *no description available*

    .. py:method:: mitsuba.Shape.ray_intersect_preliminary(overloaded)


        .. py:method:: ray_intersect_preliminary(self, ray, active=True)

            Fast ray intersection

            Efficiently test whether the shape is intersected by the given ray,
            and return preliminary information about the intersection if that is
            the case.

            If the intersection is deemed relevant (e.g. the closest to the ray
            origin), detailed intersection information can later be obtained via
            the create_surface_interaction() method.

            Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
                The ray to be tested for an intersection

            Parameter ``active`` (drjit.llvm.ad.Bool):
                Mask to specify active lanes.

            Returns → :py:obj:`mitsuba.PreliminaryIntersection`:
                *no description available*

        .. py:method:: ray_intersect_preliminary(self, ray, active=True)

            Fast ray intersection

            Efficiently test whether the shape is intersected by the given ray,
            and return preliminary information about the intersection if that is
            the case.

            If the intersection is deemed relevant (e.g. the closest to the ray
            origin), detailed intersection information can later be obtained via
            the create_surface_interaction() method.

            Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
                The ray to be tested for an intersection

            Parameter ``active`` (drjit.llvm.ad.Bool):
                Mask to specify active lanes.

            Returns → :py:obj:`mitsuba.PreliminaryIntersection`:
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

        Parameter ``it`` (:py:obj:`mitsuba.Interaction`):
            A reference position somewhere within the scene.

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D point on the domain ``[0,1]^2``

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.DirectionSample`:
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

        Returns → :py:obj:`mitsuba.PositionSample`:
            A PositionSample instance describing the generated sample

    .. py:method:: mitsuba.Shape.sensor(self)

        Return the area sensor associated with this shape (if any)

        Returns → :py:obj:`mitsuba.Sensor`:
            *no description available*

    .. py:method:: mitsuba.Shape.surface_area(self)

        Return the shape's surface area.

        The function assumes that the object is not undergoing some kind of
        time-dependent scaling.

        The default implementation throws an exception.

        Returns → drjit.llvm.ad.Float:
            *no description available*

.. py:class:: mitsuba.ShapePtr


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Shape`):
            *no description available*

    .. py:method:: mitsuba.ShapePtr.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.ShapePtr`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.bsdf(self)

        Return the shape's BSDF

        Returns → drjit::DiffArray<drjit::LLVMArray<:py:obj:`mitsuba.BSDF` const*>>:
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

        Parameter ``pi`` (:py:obj:`mitsuba.PreliminaryIntersection`):
            Data structure carrying information about the ray intersection

        Parameter ``ray_flags`` (int):
            Flags specifying which information should be computed

        Parameter ``recursion_depth``:
            Integer specifying the recursion depth for nested virtual function
            call to this method (e.g. used for instancing).

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SurfaceInteraction`:
            A data structure containing the detailed information

    .. py:method:: mitsuba.ShapePtr.emitter(self)

        Return the area emitter associated with this shape (if any)

        Returns → drjit::DiffArray<drjit::LLVMArray<:py:obj:`mitsuba.Emitter` const*>>:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → :py:obj:`mitsuba.Shape`:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.eq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.ShapePtr`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.eval_attribute(self, name, si, active=True)

        Evaluate a specific shape attribute at the given surface interaction.

        Shape attributes are user-provided fields that provide extra
        information at an intersection. An example of this would be a per-
        vertex or per-face color on a triangle mesh.

        Parameter ``name`` (str):
            Name of the attribute to evaluate

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction`):
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

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction`):
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

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction`):
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

        Returns → :py:obj:`mitsuba.SurfaceInteraction`:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.exterior_medium(self)

        Return the medium that lies on the exterior of this shape

        Returns → drjit::DiffArray<drjit::LLVMArray<:py:obj:`mitsuba.Medium` const*>>:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.gather_(source, index, mask, permute=False)

        Parameter ``source`` (:py:obj:`mitsuba.ShapePtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → :py:obj:`mitsuba.ShapePtr`:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.has_attribute(self, name, active=True)

        Returns whether this shape contains the specified attribute.

        Parameter ``name`` (str):
            Name of the attribute

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.interior_medium(self)

        Return the medium that lies on the interior of this shape

        Returns → drjit::DiffArray<drjit::LLVMArray<:py:obj:`mitsuba.Medium` const*>>:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.is_emitter(self)

        Is this shape also an area emitter?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.is_medium_transition(self)

        Does the surface of this shape mark a medium transition?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.is_sensor(self)

        Is this shape also an area sensor?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.neq_(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.ShapePtr`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.pdf_direction(self, it, active=True)

        Query the probability density of sample_direction()

        Parameter ``it`` (:py:obj:`mitsuba.Interaction`):
            A reference position somewhere within the scene.

        Parameter ``ps``:
            A position record describing the sample in question

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The probability density per unit solid angle

    .. py:method:: mitsuba.ShapePtr.pdf_position(self, ps, active=True)

        Query the probability density of sample_position() for a particular
        point on the surface.

        Parameter ``ps`` (:py:obj:`mitsuba.PositionSample`):
            A position record describing the sample in question

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The probability density per unit area

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

        Returns → :py:obj:`mitsuba.SurfaceInteraction`:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.ray_intersect_preliminary(overloaded)


        .. py:method:: ray_intersect_preliminary(self, ray, active=True)

            Fast ray intersection

            Efficiently test whether the shape is intersected by the given ray,
            and return preliminary information about the intersection if that is
            the case.

            If the intersection is deemed relevant (e.g. the closest to the ray
            origin), detailed intersection information can later be obtained via
            the create_surface_interaction() method.

            Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
                The ray to be tested for an intersection

            Parameter ``active`` (drjit.llvm.ad.Bool):
                Mask to specify active lanes.

            Returns → :py:obj:`mitsuba.PreliminaryIntersection`:
                *no description available*

        .. py:method:: ray_intersect_preliminary(self, ray, active=True)

            Fast ray intersection

            Efficiently test whether the shape is intersected by the given ray,
            and return preliminary information about the intersection if that is
            the case.

            If the intersection is deemed relevant (e.g. the closest to the ray
            origin), detailed intersection information can later be obtained via
            the create_surface_interaction() method.

            Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
                The ray to be tested for an intersection

            Parameter ``active`` (drjit.llvm.ad.Bool):
                Mask to specify active lanes.

            Returns → :py:obj:`mitsuba.PreliminaryIntersection`:
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

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.registry_get_max_()

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.registry_get_ptr_(arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.reinterpret_array_(arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → :py:obj:`mitsuba.ShapePtr`:
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

        Parameter ``it`` (:py:obj:`mitsuba.Interaction`):
            A reference position somewhere within the scene.

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D point on the domain ``[0,1]^2``

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.DirectionSample`:
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

        Returns → :py:obj:`mitsuba.PositionSample`:
            A PositionSample instance describing the generated sample

    .. py:method:: mitsuba.ShapePtr.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (:py:obj:`mitsuba.ShapePtr`):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.select_(arg0, arg1, arg2)

        Parameter ``arg0`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.ShapePtr`):
            *no description available*

        Parameter ``arg2`` (:py:obj:`mitsuba.ShapePtr`):
            *no description available*

        Returns → :py:obj:`mitsuba.ShapePtr`:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.sensor(self)

        Return the area sensor associated with this shape (if any)

        Returns → drjit::DiffArray<drjit::LLVMArray<:py:obj:`mitsuba.Sensor` const*>>:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.surface_area(self)

        Return the shape's surface area.

        The function assumes that the object is not undergoing some kind of
        time-dependent scaling.

        The default implementation throws an exception.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.zero_

        (arg0: int) -> :py:obj:`mitsuba.llvm_ad_rgb.ShapePtr`

.. py:class:: mitsuba.Spiral

    Base class: :py:obj:`mitsuba.Object`

    Generates a spiral of blocks to be rendered.

    Author:
        Adam Arbree Aug 25, 2005 RayTracer.java Used with permission.
        Copyright 2005 Program of Computer Graphics, Cornell University

    .. py:method:: __init__(self, size, block_size=32, passes=1)

        Create a new spiral generator for the given size, offset into a larger
        frame, and block size

        Parameter ``size`` (:py:obj:`mitsuba.Vector`):
            *no description available*

        Parameter ``block_size`` (int):
            *no description available*

        Parameter ``passes`` (int):
            *no description available*


    .. py:method:: mitsuba.Spiral.block_count(self)

        Return the total number of blocks

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Spiral.max_block_size(self)

        Return the maximum block size

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Spiral.next_block(self)

        Return the offset, size, and unique identifier of the next block.

        A size of zero indicates that the spiral traversal is done.

        Returns → Tuple[:py:obj:`mitsuba.Vector`, int]:
            *no description available*

    .. py:method:: mitsuba.Spiral.reset(self)

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

        Members:

        .. py:data:: EBigEndian



        .. py:data:: ELittleEndian

            PowerPC, SPARC, Motorola 68K

        .. py:data:: ENetworkByteOrder

            x86, x86_64

        .. py:method:: __init__(self, value)

            Parameter ``value`` (int):
                *no description available*


    .. py:method:: mitsuba.Stream.EByteOrder.name
        :property:

    .. py:method:: mitsuba.Stream.byte_order(self)

        Returns the byte order of this stream.

        Returns → :py:obj:`mitsuba.Stream.EByteOrder`:
            *no description available*

    .. py:method:: mitsuba.Stream.can_read(self)

        Can we read from the stream?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Stream.can_write(self)

        Can we write to the stream?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Stream.close(self)

        Closes the stream.

        No further read or write operations are permitted.

        This function is idempotent. It may be called automatically by the
        destructor.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.flush(self)

        Flushes the stream's buffers, if any

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.host_byte_order()

        Returns the byte order of the underlying machine.

        Returns → :py:obj:`mitsuba.Stream.EByteOrder`:
            *no description available*

    .. py:method:: mitsuba.Stream.read(self, arg0)

        Writes a specified amount of data into the stream. \note This does
        **not** handle endianness swapping.

        Throws an exception when not all data could be written.
        Implementations need to handle endianness swap when appropriate.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → bytes:
            *no description available*

    .. py:method:: mitsuba.Stream.read_bool(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_double(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_float(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_int16(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_int32(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_int64(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_int8(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_line(self)

        Convenience function for reading a line of text from an ASCII file

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Stream.read_single(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_string(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_uint16(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_uint32(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_uint64(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_uint8(self)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.seek(self, arg0)

        Seeks to a position inside the stream.

        Seeking beyond the size of the buffer will not modify the length of
        its contents. However, a subsequent write should start at the sought
        position and update the size appropriately.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.set_byte_order(self, arg0)

        Sets the byte order to use in this stream.

        Automatic conversion will be performed on read and write operations to
        match the system's native endianness.

        No consistency is guaranteed if this method is called after performing
        some read and write operations on the system using a different
        endianness.

        Parameter ``arg0`` (:py:obj:`mitsuba.Stream.EByteOrder`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.size(self)

        Returns the size of the stream

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Stream.skip(self, arg0)

        Skip ahead by a given number of bytes

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.tell(self)

        Gets the current position inside the stream

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Stream.truncate(self, arg0)

        Truncates the stream to a given size.

        The position is updated to ``min(old_position, size)``. Throws an
        exception if in read-only mode.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.write(self, arg0)

        Writes a specified amount of data into the stream. \note This does
        **not** handle endianness swapping.

        Throws an exception when not all data could be written.
        Implementations need to handle endianness swap when appropriate.

        Parameter ``arg0`` (bytes):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.write_bool(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_double(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (float):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_float(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (float):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_int16(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_int32(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_int64(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_int8(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_line(self, arg0)

        Convenience function for writing a line of text to an ASCII file

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.write_single(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (float):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_string(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (str):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_uint16(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_uint32(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_uint64(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_uint8(self, arg0)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → object:
            *no description available*

.. py:class:: mitsuba.StreamAppender

    Base class: :py:obj:`mitsuba.Appender`

    %Appender implementation, which writes to an arbitrary C++ output
    stream

    .. py:method:: __init__(self, arg0)

        Create a new stream appender

        Remark:
            This constructor is not exposed in the Python bindings

        Parameter ``arg0`` (str):
            *no description available*


    .. py:method:: mitsuba.StreamAppender.logs_to_file(self)

        Does this appender log to a file

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.StreamAppender.read_log(self)

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

    .. py:method:: __init__(self, pack=False, byte_order=<ByteOrder., HostByteOrder)

        Create a new ``Struct`` and indicate whether the contents are packed
        or aligned

        Parameter ``pack`` (bool):
            *no description available*

        Parameter ``byte_order`` (:py:obj:`mitsuba.Struct.ByteOrder`):
            *no description available*

        Parameter ``HostByteOrder`` (2>):
            *no description available*


    .. py:class:: mitsuba.Struct.ByteOrder

        Members:

            LittleEndian :

            BigEndian :

            HostByteOrder :

        .. py:method:: __init__(self, value)

            Parameter ``value`` (int):
                *no description available*


    .. py:method:: mitsuba.Struct.ByteOrder.name
        :property:

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

    .. py:method:: mitsuba.Struct.Field.is_float(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.Field.is_integer(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.Field.is_signed(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.Field.is_unsigned(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.Field.name
        :property:

        Name of the field

    .. py:method:: mitsuba.Struct.Field.offset
        :property:

        Offset within the ``Struct`` (in bytes)

    .. py:method:: mitsuba.Struct.Field.range(self)

        Returns → Tuple[float, float]:
            *no description available*

    .. py:method:: mitsuba.Struct.Field.size
        :property:

        Size in bytes

    .. py:method:: mitsuba.Struct.Field.type
        :property:

        Type identifier

    .. py:class:: mitsuba.Struct.Flags

        Members:

        .. py:data:: Empty

            No flags set (default value)

        .. py:data:: Normalized

            Specifies whether an integer field encodes a normalized value in the
            range [0, 1]. The flag is ignored if specified for floating point
            valued fields.

        .. py:data:: Gamma

            Specifies whether the field encodes a sRGB gamma-corrected value.
            Assumes ``Normalized`` is also specified.

        .. py:data:: Weight

            In FieldConverter::convert, when an input structure contains a weight
            field, the value of all entries are considered to be expressed
            relative to its value. Converting to an un-weighted structure entails
            a division by the weight.

        .. py:data:: Assert

            In FieldConverter::convert, check that the field value matches the
            specified default value. Otherwise, return a failure

        .. py:data:: Alpha

            Specifies whether the field encodes an alpha value

        .. py:data:: PremultipliedAlpha

            Specifies whether the field encodes an alpha premultiplied value

        .. py:data:: Default

            In FieldConverter::convert, when the field is missing in the source
            record, replace it by the specified default value

        .. py:method:: __init__(self, value)

            Parameter ``value`` (int):
                *no description available*


    .. py:method:: mitsuba.Struct.Flags.name
        :property:

    .. py:class:: mitsuba.Struct.Type

        Members:

            Int8 :

            UInt8 :

            Int16 :

            UInt16 :

            Int32 :

            UInt32 :

            Int64 :

            UInt64 :

            Float16 :

            Float32 :

            Float64 :

            Invalid :


        .. py:method:: __init__(self, value)

            Parameter ``value`` (int):
                *no description available*

        .. py:method:: __init__(self, dtype)

            Parameter ``dtype`` (dtype):
                *no description available*

    .. py:method:: mitsuba.Struct.Type.name
        :property:

    .. py:method:: mitsuba.Struct.alignment(self)

        Return the alignment (in bytes) of the data structure

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Struct.append(self, name, type, flags=<Flags., Empty, default=0.0)

        Append a new field to the ``Struct``; determines size and offset
        automatically

        Parameter ``name`` (str):
            *no description available*

        Parameter ``type`` (:py:obj:`mitsuba.Struct.Type`):
            *no description available*

        Parameter ``flags`` (int):
            *no description available*

        Parameter ``Empty`` (0>):
            *no description available*

        Parameter ``default`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.Struct`:
            *no description available*

    .. py:method:: mitsuba.Struct.byte_order(self)

        Return the byte order of the ``Struct``

        Returns → :py:obj:`mitsuba.Struct.ByteOrder`:
            *no description available*

    .. py:method:: mitsuba.Struct.dtype(self)

        Return a NumPy dtype corresponding to this data structure

        Returns → dtype:
            *no description available*

    .. py:method:: mitsuba.Struct.field(self, arg0)

        Look up a field by name (throws an exception if not found)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → :py:obj:`mitsuba.Struct.Field`:
            *no description available*

    .. py:method:: mitsuba.Struct.field_count(self)

        Return the number of fields

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Struct.has_field(self, arg0)

        Check if the ``Struct`` has a field of the specified name

        Parameter ``arg0`` (str):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.is_float(arg0)

        Check whether the given type is a floating point type

        Parameter ``arg0`` (:py:obj:`mitsuba.Struct.Type`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.is_integer(arg0)

        Check whether the given type is an integer type

        Parameter ``arg0`` (:py:obj:`mitsuba.Struct.Type`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.is_signed(arg0)

        Check whether the given type is a signed type

        Parameter ``arg0`` (:py:obj:`mitsuba.Struct.Type`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.is_unsigned(arg0)

        Check whether the given type is an unsigned type

        Parameter ``arg0`` (:py:obj:`mitsuba.Struct.Type`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.range(arg0)

        Return the representable range of the given type

        Parameter ``arg0`` (:py:obj:`mitsuba.Struct.Type`):
            *no description available*

        Returns → Tuple[float, float]:
            *no description available*

    .. py:method:: mitsuba.Struct.size(self)

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


    .. py:method:: mitsuba.StructConverter.convert(self, arg0)

        Parameter ``arg0`` (bytes):
            *no description available*

        Returns → bytes:
            *no description available*

    .. py:method:: mitsuba.StructConverter.source(self)

        Return the source ``Struct`` descriptor

        Returns → :py:obj:`mitsuba.Struct`:
            *no description available*

    .. py:method:: mitsuba.StructConverter.target(self)

        Return the target ``Struct`` descriptor

        Returns → :py:obj:`mitsuba.Struct`:
            *no description available*

.. py:class:: mitsuba.SurfaceInteraction3f

    Base class: :py:obj:`mitsuba.Interaction3f`

    Stores information related to a surface scattering interaction


    .. py:method:: __init__(self)

        Construct from a position sample. Unavailable fields such as `wi` and
        the partial derivatives are left uninitialized. The `shape` pointer is
        left uninitialized because we can't guarantee that the given
        PositionSample::object points to a Shape instance.

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            *no description available*

    .. py:method:: __init__(self, ps, wavelengths)

        Construct from a position sample. Unavailable fields such as `wi` and
        the partial derivatives are left uninitialized. The `shape` pointer is
        left uninitialized because we can't guarantee that the given
        PositionSample::object points to a Shape instance.

        Parameter ``ps`` (:py:obj:`mitsuba.PositionSample`):
            *no description available*

        Parameter ``wavelengths`` (:py:obj:`mitsuba.Color0f`):
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.boundary_test
        :property:

        Boundary-test value used in reparameterized integrators, a soft
        indicator function which returns a zero value at the silhouette of the
        shape from the perspective of a given ray. Everywhere else this
        function will return non-negative values reflecting the distance of
        the surface interaction to this closest point on the silhouette.

    .. py:method:: mitsuba.SurfaceInteraction3f.bsdf(overloaded)


        .. py:method:: bsdf(self, ray)

            Returns the BSDF of the intersected shape.

            The parameter ``ray`` must match the one used to create the
            interaction record. This function computes texture coordinate partials
            if this is required by the BSDF (e.g. for texture filtering).

            Implementation in 'bsdf.h'

            Parameter ``ray`` (:py:obj:`mitsuba.RayDifferential3f`):
                *no description available*

            Returns → drjit::DiffArray<drjit::LLVMArray<:py:obj:`mitsuba.BSDF` const*>>:
                *no description available*

        .. py:method:: bsdf(self)

            Returns → drjit::DiffArray<drjit::LLVMArray<:py:obj:`mitsuba.BSDF` const*>>:
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

    .. py:method:: mitsuba.SurfaceInteraction3f.has_n_partials(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.has_uv_partials(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.initialize_sh_frame(self)

        Initialize local shading frame using Gram-schmidt orthogonalization

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.instance
        :property:

        Stores a pointer to the parent instance (if applicable)

    .. py:method:: mitsuba.SurfaceInteraction3f.is_medium_transition(self)

        Does the surface mark a transition between two media?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.is_sensor(self)

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

    .. py:method:: mitsuba.SurfaceInteraction3f.target_medium(overloaded)


        .. py:method:: target_medium(self, d)

            Determine the target medium

            When ``is_medium_transition()`` = ``True``, determine the medium that
            contains the ``ray(this->p, d)``

            Parameter ``d`` (:py:obj:`mitsuba.Vector3f`):
                *no description available*

            Returns → :py:obj:`mitsuba.MediumPtr`:
                *no description available*

        .. py:method:: target_medium(self, cos_theta)

            Determine the target medium based on the cosine of the angle between
            the geometric normal and a direction

            Returns the exterior medium when ``cos_theta > 0`` and the interior
            medium when ``cos_theta <= 0``.

            Parameter ``cos_theta`` (drjit.llvm.ad.Float):
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


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, array)

        Parameter ``array`` (object):
            *no description available*

    .. py:method:: __init__(self, array)

        Parameter ``array`` (drjit.llvm.ad.Bool):
            *no description available*

    .. py:method:: __init__(self, array, shape)

        Parameter ``array`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``shape`` (List[int]):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXb):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.TensorXb):
            *no description available*

    .. py:method:: mitsuba.TensorXb.and_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXb):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXb.andnot_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXb):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXb.assign(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXb):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXb.data_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.TensorXb.eq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXb):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXb.iand_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXb):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXb.ior_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXb):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXb.ixor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXb):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXb.neq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXb):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXb.not_(self)

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXb.or_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXb):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXb.select_

        (arg0: drjit.llvm.ad.TensorXb, arg1: drjit.llvm.ad.TensorXb, arg2: drjit.llvm.ad.TensorXb) -> drjit.llvm.ad.TensorXb

    .. py:method:: mitsuba.TensorXb.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXb.set_index_ad_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXb.xor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXb):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

.. py:class:: mitsuba.TensorXf


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, array)

        Parameter ``array`` (object):
            *no description available*

    .. py:method:: __init__(self, array)

        Parameter ``array`` (drjit.llvm.ad.Float):
            *no description available*

    .. py:method:: __init__(self, array, shape)

        Parameter ``array`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``shape`` (List[int]):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: drjit.llvm.ad.TensorXf, arg0: drjit.llvm.ad.TensorXf64) -> None

        11. __init__(self: drjit.llvm.ad.TensorXf, arg0: drjit.llvm.TensorXf) -> None

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

    .. py:method:: mitsuba.TensorXf.abs_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.acos_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.acosh_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.add_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.and_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.andnot_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.asin_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.asinh_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.assign(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXf.atan2_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.atan_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.atanh_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.cbrt_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.ceil_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.cos_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.cosh_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.cot_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.csc_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.data_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.TensorXf.eq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXf.erf_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.erfinv_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.exp2_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.exp_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.floor_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.fma_(self, arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.ge_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXf.gt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXf.iadd_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.iand_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.imul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.ior_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.isub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.itruediv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.ixor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.le_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXf.lgamma_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.log2_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.log_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.lt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXf.maximum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.minimum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.mod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.mul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.neg_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.neq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXf.not_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.or_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.rcp_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.reinterpret_array_(overloaded)


        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
                *no description available*

            Returns → drjit.llvm.ad.TensorXf:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
                *no description available*

            Returns → drjit.llvm.ad.TensorXf:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
                *no description available*

            Returns → drjit.llvm.ad.TensorXf:
                *no description available*

    .. py:method:: mitsuba.TensorXf.round_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.rsqrt_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.sec_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.select_

        (arg0: drjit.llvm.ad.TensorXb, arg1: drjit.llvm.ad.TensorXf, arg2: drjit.llvm.ad.TensorXf) -> drjit.llvm.ad.TensorXf

    .. py:method:: mitsuba.TensorXf.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXf.set_index_ad_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXf.sin_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.sincos_(self)

        Returns → Tuple[drjit.llvm.ad.TensorXf, drjit.llvm.ad.TensorXf]:
            *no description available*

    .. py:method:: mitsuba.TensorXf.sincosh_(self)

        Returns → Tuple[drjit.llvm.ad.TensorXf, drjit.llvm.ad.TensorXf]:
            *no description available*

    .. py:method:: mitsuba.TensorXf.sinh_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.sqrt_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.sub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.tan_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.tanh_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.tgamma_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.truediv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.trunc_(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.TensorXf.xor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

.. py:class:: mitsuba.TensorXi


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, array)

        Parameter ``array`` (object):
            *no description available*

    .. py:method:: __init__(self, array)

        Parameter ``array`` (drjit.llvm.ad.Int):
            *no description available*

    .. py:method:: __init__(self, array, shape)

        Parameter ``array`` (drjit.llvm.ad.Int):
            *no description available*

        Parameter ``shape`` (List[int]):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: drjit.llvm.ad.TensorXi, arg0: drjit.llvm.ad.TensorXf64) -> None

        11. __init__(self: drjit.llvm.ad.TensorXi, arg0: drjit.llvm.TensorXi) -> None

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

    .. py:method:: mitsuba.TensorXi.abs_(self)

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.add_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.and_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.andnot_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.assign(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXi.data_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.TensorXi.eq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXi.floordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.fma_(self, arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.ge_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXi.gt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXi.iadd_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.iand_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.ifloordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.imod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.imul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.ior_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.isl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.isr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.isub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.ixor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.le_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXi.lt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXi.lzcnt_(self)

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.maximum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.minimum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.mod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.mul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.mulhi_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.neg_(self)

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.neq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXi.not_(self)

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.or_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.popcnt_(self)

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.reinterpret_array_(overloaded)


        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
                *no description available*

            Returns → drjit.llvm.ad.TensorXi:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
                *no description available*

            Returns → drjit.llvm.ad.TensorXi:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
                *no description available*

            Returns → drjit.llvm.ad.TensorXi:
                *no description available*

    .. py:method:: mitsuba.TensorXi.select_

        (arg0: drjit.llvm.ad.TensorXb, arg1: drjit.llvm.ad.TensorXi, arg2: drjit.llvm.ad.TensorXi) -> drjit.llvm.ad.TensorXi

    .. py:method:: mitsuba.TensorXi.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXi.set_index_ad_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXi.sl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.sr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.sub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.tzcnt_(self)

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

    .. py:method:: mitsuba.TensorXi.xor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi:
            *no description available*

.. py:class:: mitsuba.TensorXi64


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, array)

        Parameter ``array`` (object):
            *no description available*

    .. py:method:: __init__(self, array)

        Parameter ``array`` (drjit.llvm.ad.Int64):
            *no description available*

    .. py:method:: __init__(self, array, shape)

        Parameter ``array`` (drjit.llvm.ad.Int64):
            *no description available*

        Parameter ``shape`` (List[int]):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: drjit.llvm.ad.TensorXi64, arg0: drjit.llvm.ad.TensorXf64) -> None

        11. __init__(self: drjit.llvm.ad.TensorXi64, arg0: drjit.llvm.TensorXi64) -> None

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

    .. py:method:: mitsuba.TensorXi64.abs_(self)

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.add_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.and_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.andnot_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.assign(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.data_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.eq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.floordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.fma_(self, arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.ge_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.gt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.iadd_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.iand_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.ifloordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.imod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.imul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.ior_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.isl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.isr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.isub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.ixor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.le_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.lt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.lzcnt_(self)

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.maximum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.minimum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.mod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.mul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.mulhi_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.neg_(self)

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.neq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.not_(self)

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.or_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.popcnt_(self)

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.reinterpret_array_(overloaded)


        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
                *no description available*

            Returns → drjit.llvm.ad.TensorXi64:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
                *no description available*

            Returns → drjit.llvm.ad.TensorXi64:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXf64):
                *no description available*

            Returns → drjit.llvm.ad.TensorXi64:
                *no description available*

    .. py:method:: mitsuba.TensorXi64.select_

        (arg0: drjit.llvm.ad.TensorXb, arg1: drjit.llvm.ad.TensorXi64, arg2: drjit.llvm.ad.TensorXi64) -> drjit.llvm.ad.TensorXi64

    .. py:method:: mitsuba.TensorXi64.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.set_index_ad_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.sl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.sr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.sub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.tzcnt_(self)

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

    .. py:method:: mitsuba.TensorXi64.xor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXi64:
            *no description available*

.. py:class:: mitsuba.TensorXu


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, array)

        Parameter ``array`` (object):
            *no description available*

    .. py:method:: __init__(self, array)

        Parameter ``array`` (drjit.llvm.ad.UInt):
            *no description available*

    .. py:method:: __init__(self, array, shape)

        Parameter ``array`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``shape`` (List[int]):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: drjit.llvm.ad.TensorXu, arg0: drjit.llvm.ad.TensorXf64) -> None

        11. __init__(self: drjit.llvm.ad.TensorXu, arg0: drjit.llvm.TensorXu) -> None

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

    .. py:method:: mitsuba.TensorXu.abs_(self)

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.add_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.and_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.andnot_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.assign(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXu.data_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.TensorXu.eq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXu.floordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.fma_(self, arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.ge_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXu.gt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXu.iadd_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.iand_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.ifloordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.imod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.imul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.ior_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.isl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.isr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.isub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.ixor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.le_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXu.lt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXu.lzcnt_(self)

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.maximum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.minimum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.mod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.mul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.mulhi_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.neg_(self)

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.neq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXu.not_(self)

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.or_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.popcnt_(self)

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.reinterpret_array_(overloaded)


        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
                *no description available*

            Returns → drjit.llvm.ad.TensorXu:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
                *no description available*

            Returns → drjit.llvm.ad.TensorXu:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
                *no description available*

            Returns → drjit.llvm.ad.TensorXu:
                *no description available*

    .. py:method:: mitsuba.TensorXu.select_

        (arg0: drjit.llvm.ad.TensorXb, arg1: drjit.llvm.ad.TensorXu, arg2: drjit.llvm.ad.TensorXu) -> drjit.llvm.ad.TensorXu

    .. py:method:: mitsuba.TensorXu.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXu.set_index_ad_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXu.sl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.sr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.sub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.tzcnt_(self)

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

    .. py:method:: mitsuba.TensorXu.xor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu:
            *no description available*

.. py:class:: mitsuba.TensorXu64


    .. py:method:: __init__(self)

    .. py:method:: __init__(self, array)

        Parameter ``array`` (object):
            *no description available*

    .. py:method:: __init__(self, array)

        Parameter ``array`` (drjit.llvm.ad.UInt64):
            *no description available*

    .. py:method:: __init__(self, array, shape)

        Parameter ``array`` (drjit.llvm.ad.UInt64):
            *no description available*

        Parameter ``shape`` (List[int]):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: drjit.llvm.ad.TensorXu64, arg0: drjit.llvm.ad.TensorXf64) -> None

        11. __init__(self: drjit.llvm.ad.TensorXu64, arg0: drjit.llvm.TensorXu64) -> None

        Parameter ``arg0`` (drjit.llvm.ad.TensorXf):
            *no description available*

    .. py:method:: mitsuba.TensorXu64.abs_(self)

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.add_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.and_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.andnot_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.assign(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.data_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.eq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.floordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.fma_(self, arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.ge_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.gt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.iadd_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.iand_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.ifloordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.imod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.imul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.ior_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.isl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.isr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.isub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.ixor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.le_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.lt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.lzcnt_(self)

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.maximum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.minimum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.mod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.mul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.mulhi_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.neg_(self)

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.neq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXb:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.not_(self)

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.or_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.popcnt_(self)

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.reinterpret_array_(overloaded)


        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXi64):
                *no description available*

            Returns → drjit.llvm.ad.TensorXu64:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
                *no description available*

            Returns → drjit.llvm.ad.TensorXu64:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.TensorXf64):
                *no description available*

            Returns → drjit.llvm.ad.TensorXu64:
                *no description available*

    .. py:method:: mitsuba.TensorXu64.select_

        (arg0: drjit.llvm.ad.TensorXb, arg1: drjit.llvm.ad.TensorXu64, arg2: drjit.llvm.ad.TensorXu64) -> drjit.llvm.ad.TensorXu64

    .. py:method:: mitsuba.TensorXu64.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.set_index_ad_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.sl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.sr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.sub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.tzcnt_(self)

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

    .. py:method:: mitsuba.TensorXu64.xor_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.TensorXu64):
            *no description available*

        Returns → drjit.llvm.ad.TensorXu64:
            *no description available*

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


    .. py:method:: mitsuba.Texture.D65(scale=1.0)

        Parameter ``scale`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.Texture`:
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

    .. py:method:: mitsuba.Texture.is_spatially_varying(self)

        Does this texture evaluation depend on the UV coordinates

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture.max(self)

        Return the maximum value of the spectrum

        Not every implementation necessarily provides this function. The
        default implementation throws an exception.

        Even if the operation is provided, it may only return an
        approximation.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.Texture.mean(self)

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

    .. py:method:: mitsuba.Texture.resolution(self)

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

        Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

        Returns → Tuple[:py:obj:`mitsuba.Color0f`, :py:obj:`mitsuba.Color3f`]:
            1. Set of sampled wavelengths specified in nanometers

        2. The Monte Carlo importance weight (Spectral power distribution
        value divided by the sampling density)

    .. py:method:: mitsuba.Texture.spectral_resolution(self)

        Returns the resolution of the spectrum in nanometers (if discretized)

        Not every implementation necessarily provides this function. The
        default implementation throws an exception.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.Texture.wavelength_range(self)

        Returns the range of wavelengths covered by the spectrum

        The default implementation returns ``(MI_CIE_MIN, MI_CIE_MAX)``

        Returns → :py:obj:`mitsuba.ScalarVector2f`:
            *no description available*

.. py:class:: mitsuba.Texture1f


    .. py:method:: __init__(self, shape, channels, use_accel=True, filter_mode=<FilterMode., Linear, wrap_mode=<WrapMode., Clamp)

        Parameter ``shape`` (List[int[1]]):
            *no description available*

        Parameter ``channels`` (int):
            *no description available*

        Parameter ``use_accel`` (bool):
            *no description available*

        Parameter ``filter_mode`` (drjit.FilterMode):
            *no description available*

        Parameter ``Linear`` (1>):
            *no description available*

        Parameter ``wrap_mode`` (drjit.WrapMode):
            *no description available*

        Parameter ``Clamp`` (1>):
            *no description available*

    .. py:method:: __init__(self, tensor, use_accel=True, migrate=True, filter_mode=<FilterMode., Linear, wrap_mode=<WrapMode., Clamp)

        Parameter ``tensor`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``use_accel`` (bool):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Parameter ``filter_mode`` (drjit.FilterMode):
            *no description available*

        Parameter ``Linear`` (1>):
            *no description available*

        Parameter ``wrap_mode`` (drjit.WrapMode):
            *no description available*

        Parameter ``Clamp`` (1>):
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval_cubic(self, pos, active=True, force_drjit=False)

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Parameter ``force_drjit`` (bool):
            *no description available*

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval_cubic_grad(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[List[drjit.llvm.ad.Float], List[drjit.llvm.ad.Array1f]]:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval_cubic_hessian(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[List[drjit.llvm.ad.Float], List[drjit.llvm.ad.Array1f], List[drjit::Matrix<drjit::DiffArray<drjit::LLVMArray<float>>, 1ul>]]:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval_cuda(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval_fetch(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[List[drjit.llvm.ad.Float][2]]:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval_fetch_cuda(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[List[drjit.llvm.ad.Float][2]]:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval_fetch_drjit(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[List[drjit.llvm.ad.Float][2]]:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval_nonaccel(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture1f.filter_mode(self)

        Returns → drjit.FilterMode:
            *no description available*

    .. py:method:: mitsuba.Texture1f.migrated(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture1f.set_tensor(self, tensor, migrate=False)

        Parameter ``tensor`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Texture1f.set_value(self, value, migrate=False)

        Parameter ``value`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Texture1f.tensor(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.Texture1f.use_accel(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture1f.value(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Texture1f.wrap_mode(self)

        Returns → drjit.WrapMode:
            *no description available*

.. py:class:: mitsuba.Texture2f


    .. py:method:: __init__(self, shape, channels, use_accel=True, filter_mode=<FilterMode., Linear, wrap_mode=<WrapMode., Clamp)

        Parameter ``shape`` (List[int[2]]):
            *no description available*

        Parameter ``channels`` (int):
            *no description available*

        Parameter ``use_accel`` (bool):
            *no description available*

        Parameter ``filter_mode`` (drjit.FilterMode):
            *no description available*

        Parameter ``Linear`` (1>):
            *no description available*

        Parameter ``wrap_mode`` (drjit.WrapMode):
            *no description available*

        Parameter ``Clamp`` (1>):
            *no description available*

    .. py:method:: __init__(self, tensor, use_accel=True, migrate=True, filter_mode=<FilterMode., Linear, wrap_mode=<WrapMode., Clamp)

        Parameter ``tensor`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``use_accel`` (bool):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Parameter ``filter_mode`` (drjit.FilterMode):
            *no description available*

        Parameter ``Linear`` (1>):
            *no description available*

        Parameter ``wrap_mode`` (drjit.WrapMode):
            *no description available*

        Parameter ``Clamp`` (1>):
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval_cubic(self, pos, active=True, force_drjit=False)

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Parameter ``force_drjit`` (bool):
            *no description available*

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval_cubic_grad(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[List[drjit.llvm.ad.Float], List[drjit.llvm.ad.Array2f]]:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval_cubic_hessian(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[List[drjit.llvm.ad.Float], List[drjit.llvm.ad.Array2f], List[drjit.llvm.ad.Matrix2f]]:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval_cuda(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval_fetch(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[List[drjit.llvm.ad.Float][4]]:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval_fetch_cuda(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[List[drjit.llvm.ad.Float][4]]:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval_fetch_drjit(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[List[drjit.llvm.ad.Float][4]]:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval_nonaccel(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture2f.filter_mode(self)

        Returns → drjit.FilterMode:
            *no description available*

    .. py:method:: mitsuba.Texture2f.migrated(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture2f.set_tensor(self, tensor, migrate=False)

        Parameter ``tensor`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Texture2f.set_value(self, value, migrate=False)

        Parameter ``value`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Texture2f.tensor(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.Texture2f.use_accel(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture2f.value(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Texture2f.wrap_mode(self)

        Returns → drjit.WrapMode:
            *no description available*

.. py:class:: mitsuba.Texture3f


    .. py:method:: __init__(self, shape, channels, use_accel=True, filter_mode=<FilterMode., Linear, wrap_mode=<WrapMode., Clamp)

        Parameter ``shape`` (List[int[3]]):
            *no description available*

        Parameter ``channels`` (int):
            *no description available*

        Parameter ``use_accel`` (bool):
            *no description available*

        Parameter ``filter_mode`` (drjit.FilterMode):
            *no description available*

        Parameter ``Linear`` (1>):
            *no description available*

        Parameter ``wrap_mode`` (drjit.WrapMode):
            *no description available*

        Parameter ``Clamp`` (1>):
            *no description available*

    .. py:method:: __init__(self, tensor, use_accel=True, migrate=True, filter_mode=<FilterMode., Linear, wrap_mode=<WrapMode., Clamp)

        Parameter ``tensor`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``use_accel`` (bool):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Parameter ``filter_mode`` (drjit.FilterMode):
            *no description available*

        Parameter ``Linear`` (1>):
            *no description available*

        Parameter ``wrap_mode`` (drjit.WrapMode):
            *no description available*

        Parameter ``Clamp`` (1>):
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval_cubic(self, pos, active=True, force_drjit=False)

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Parameter ``force_drjit`` (bool):
            *no description available*

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval_cubic_grad(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[List[drjit.llvm.ad.Float], List[drjit.llvm.ad.Array3f]]:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval_cubic_hessian(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[List[drjit.llvm.ad.Float], List[drjit.llvm.ad.Array3f], List[drjit.llvm.ad.Matrix3f]]:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval_cuda(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval_fetch(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[List[drjit.llvm.ad.Float][8]]:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval_fetch_cuda(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[List[drjit.llvm.ad.Float][8]]:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval_fetch_drjit(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[List[drjit.llvm.ad.Float][8]]:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval_nonaccel(self, pos, active=True)

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture3f.filter_mode(self)

        Returns → drjit.FilterMode:
            *no description available*

    .. py:method:: mitsuba.Texture3f.migrated(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture3f.set_tensor(self, tensor, migrate=False)

        Parameter ``tensor`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Texture3f.set_value(self, value, migrate=False)

        Parameter ``value`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Texture3f.tensor(self)

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.Texture3f.use_accel(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture3f.value(self)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Texture3f.wrap_mode(self)

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

        Members:

        .. py:data:: EIdlePriority



        .. py:data:: ELowestPriority



        .. py:data:: ELowPriority



        .. py:data:: ENormalPriority



        .. py:data:: EHighPriority



        .. py:data:: EHighestPriority



        .. py:data:: ERealtimePriority



        .. py:method:: __init__(self, value)

            Parameter ``value`` (int):
                *no description available*


    .. py:method:: mitsuba.Thread.EPriority.name
        :property:

    .. py:method:: mitsuba.Thread.core_affinity(self)

        Return the core affinity

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Thread.detach(self)

        Detach the thread and release resources

        After a call to this function, join() cannot be used anymore. This
        releases resources, which would otherwise be held until a call to
        join().

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.file_resolver(self)

        Return the file resolver associated with the current thread

        Returns → :py:obj:`mitsuba.FileResolver`:
            *no description available*

    .. py:method:: mitsuba.Thread.is_critical(self)

        Return the value of the critical flag

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Thread.is_running(self)

        Is this thread still running?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Thread.join(self)

        Wait until the thread finishes

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.logger(self)

        Return the thread's logger instance

        Returns → :py:obj:`mitsuba.Logger`:
            *no description available*

    .. py:method:: mitsuba.Thread.name(self)

        Return the name of this thread

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Thread.parent(self)

        Return the parent thread

        Returns → :py:obj:`mitsuba.Thread`:
            *no description available*

    .. py:method:: mitsuba.Thread.priority(self)

        Return the thread priority

        Returns → :py:obj:`mitsuba.Thread.EPriority`:
            *no description available*

    .. py:method:: mitsuba.Thread.register_external_thread(arg0)

        Register a new thread (e.g. Dr.Jit, Python) with Mitsuba thread
        system. Returns true upon success.

        Parameter ``arg0`` (str):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Thread.set_core_affinity(self, arg0)

        Set the core affinity

        This function provides a hint to the operating system scheduler that
        the thread should preferably run on the specified processor core. By
        default, the parameter is set to -1, which means that there is no
        affinity.

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.set_critical(self, arg0)

        Specify whether or not this thread is critical

        When an thread marked critical crashes from an uncaught exception, the
        whole process is brought down. The default is ``False``.

        Parameter ``arg0`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.set_file_resolver(self, arg0)

        Set the file resolver associated with the current thread

        Parameter ``arg0`` (:py:obj:`mitsuba.FileResolver`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.set_logger(self, arg0)

        Set the logger instance used to process log messages from this thread

        Parameter ``arg0`` (:py:obj:`mitsuba.Logger`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.set_name(self, arg0)

        Set the name of this thread

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.set_priority(self, arg0)

        Set the thread priority

        This does not always work -- for instance, Linux requires root
        privileges for this operation.

        Parameter ``arg0`` (:py:obj:`mitsuba.Thread.EPriority`):
            *no description available*

        Returns → bool:
            ``True`` upon success.

    .. py:method:: mitsuba.Thread.sleep(arg0)

        Sleep for a certain amount of time (in milliseconds)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.start(self)

        Start the thread

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.thread()

        Return the current thread

        Returns → :py:obj:`mitsuba.Thread`:
            *no description available*

    .. py:method:: mitsuba.Thread.thread_id()

        Return a unique ID that is associated with this thread

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Thread.wait_for_tasks()

        Wait for previously registered nanothread tasks to complete

        Returns → None:
            *no description available*

.. py:class:: mitsuba.ThreadEnvironment

    Captures a thread environment (logger and file resolver). Used with
    ScopedSetThreadEnvironment

    .. py:method:: __init__(self)


.. py:class:: mitsuba.Timer

    .. py:method:: mitsuba.Timer.begin_stage(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Timer.end_stage(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Timer.reset(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Timer.value(self)

        Returns → int:
            *no description available*

.. py:class:: mitsuba.Transform3d

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)


    .. py:method:: __init__(self)

        Initialize with the identity matrix

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.Transform3d`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (numpy.ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Initialize the transformation from the given matrix (and compute its
        inverse transpose)

        Parameter ``arg0`` (drjit.llvm.ad.Matrix3f64):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1)

        Initialize from a matrix and its inverse transpose

        Parameter ``arg0`` (drjit.llvm.ad.Matrix3f64):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Matrix3f64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Broadcast constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.Transform`):
            *no description available*

    .. py:method:: mitsuba.Transform3d.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Transform3d`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Transform3d.has_scale(overloaded)


        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → drjit.llvm.ad.Bool:
                *no description available*

        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → drjit.llvm.ad.Bool:
                *no description available*

    .. py:method:: mitsuba.Transform3d.inverse(self)

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.Transform3d`:
            *no description available*

    .. py:method:: mitsuba.Transform3d.rotate(angle)

        Create a rotation transformation in 2D. The angle is specified in
        degrees

        Parameter ``angle`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<double>>, 3>:
            *no description available*

    .. py:method:: mitsuba.Transform3d.scale(v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point2d`):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<double>>, 3>:
            *no description available*

    .. py:method:: mitsuba.Transform3d.transform_affine(overloaded)


        .. py:method:: transform_affine(self, p)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``p`` (:py:obj:`mitsuba.Point2d`):
                *no description available*

            Returns → :py:obj:`mitsuba.Point2d`:
                *no description available*

        .. py:method:: transform_affine(self, v)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``v`` (:py:obj:`mitsuba.Vector2d`):
                *no description available*

            Returns → :py:obj:`mitsuba.Vector2d`:
                *no description available*

    .. py:method:: mitsuba.Transform3d.translate(v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point2d`):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<double>>, 3>:
            *no description available*

    .. py:method:: mitsuba.Transform3d.translation(self)

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


    .. py:method:: __init__(self)

        Initialize with the identity matrix

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.Transform3f`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (numpy.ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Initialize the transformation from the given matrix (and compute its
        inverse transpose)

        Parameter ``arg0`` (drjit.llvm.ad.Matrix3f):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1)

        Initialize from a matrix and its inverse transpose

        Parameter ``arg0`` (drjit.llvm.ad.Matrix3f):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Matrix3f):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Broadcast constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.Transform`):
            *no description available*

    .. py:method:: mitsuba.Transform3f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Transform3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Transform3f.has_scale(overloaded)


        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → drjit.llvm.ad.Bool:
                *no description available*

        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → drjit.llvm.ad.Bool:
                *no description available*

    .. py:method:: mitsuba.Transform3f.inverse(self)

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.Transform3f`:
            *no description available*

    .. py:method:: mitsuba.Transform3f.rotate(angle)

        Create a rotation transformation in 2D. The angle is specified in
        degrees

        Parameter ``angle`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<float>>, 3>:
            *no description available*

    .. py:method:: mitsuba.Transform3f.scale(v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<float>>, 3>:
            *no description available*

    .. py:method:: mitsuba.Transform3f.transform_affine(overloaded)


        .. py:method:: transform_affine(self, p)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``p`` (:py:obj:`mitsuba.Point2f`):
                *no description available*

            Returns → :py:obj:`mitsuba.Point2f`:
                *no description available*

        .. py:method:: transform_affine(self, v)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``v`` (:py:obj:`mitsuba.Vector2f`):
                *no description available*

            Returns → :py:obj:`mitsuba.Vector2f`:
                *no description available*

    .. py:method:: mitsuba.Transform3f.translate(v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<float>>, 3>:
            *no description available*

    .. py:method:: mitsuba.Transform3f.translation(self)

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


    .. py:method:: __init__(self)

        Initialize with the identity matrix

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.Transform4d`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (numpy.ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Initialize the transformation from the given matrix (and compute its
        inverse transpose)

        Parameter ``arg0`` (drjit.llvm.ad.Matrix4f64):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1)

        Initialize from a matrix and its inverse transpose

        Parameter ``arg0`` (drjit.llvm.ad.Matrix4f64):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Matrix4f64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Broadcast constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.Transform`):
            *no description available*

    .. py:method:: mitsuba.Transform4d.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Transform4d`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Transform4d.extract(self)

        Extract a lower-dimensional submatrix

        Returns → :py:obj:`mitsuba.Transform3d`:
            *no description available*

    .. py:method:: mitsuba.Transform4d.from_frame(frame)

        Creates a transformation that converts from 'frame' to the standard
        basis

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<double>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4d.has_scale(overloaded)


        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → drjit.llvm.ad.Bool:
                *no description available*

        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → drjit.llvm.ad.Bool:
                *no description available*

    .. py:method:: mitsuba.Transform4d.inverse(self)

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.Transform4d`:
            *no description available*

    .. py:method:: mitsuba.Transform4d.look_at(origin, target, up)

        Create a look-at camera transformation

        Parameter ``origin`` (:py:obj:`mitsuba.Point3d`):
            Camera position

        Parameter ``target`` (:py:obj:`mitsuba.Point3d`):
            Target vector

        Parameter ``up`` (:py:obj:`mitsuba.Point3d`):
            Up vector

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<double>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4d.orthographic(near, far)

        Create an orthographic transformation, which maps Z to [0,1] and
        leaves the X and Y coordinates untouched.

        Parameter ``near`` (drjit.llvm.ad.Float64):
            Near clipping plane

        Parameter ``far`` (drjit.llvm.ad.Float64):
            Far clipping plane

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<double>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4d.perspective(fov, near, far)

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

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<double>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4d.rotate(axis, angle)

        Create a rotation transformation around an arbitrary axis in 3D. The
        angle is specified in degrees

        Parameter ``axis`` (:py:obj:`mitsuba.Point3d`):
            *no description available*

        Parameter ``angle`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<double>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4d.scale(v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point3d`):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<double>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4d.to_frame(frame)

        Creates a transformation that converts from the standard basis to
        'frame'

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<double>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4d.transform_affine(overloaded)


        .. py:method:: transform_affine(self, p)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``p`` (:py:obj:`mitsuba.Point3d`):
                *no description available*

            Returns → :py:obj:`mitsuba.Point3d`:
                *no description available*

        .. py:method:: transform_affine(self, ray)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``ray`` (:py:obj:`mitsuba.Ray`):
                *no description available*

            Returns → :py:obj:`mitsuba.Ray`:
                *no description available*

        .. py:method:: transform_affine(self, v)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``v`` (:py:obj:`mitsuba.Vector3d`):
                *no description available*

            Returns → :py:obj:`mitsuba.Vector3d`:
                *no description available*

        .. py:method:: transform_affine(self, n)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``n`` (:py:obj:`mitsuba.Normal3d`):
                *no description available*

            Returns → :py:obj:`mitsuba.Normal3d`:
                *no description available*

    .. py:method:: mitsuba.Transform4d.translate(v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point3d`):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<double>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4d.translation(self)

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


    .. py:method:: __init__(self)

        Initialize with the identity matrix

    .. py:method:: __init__(self, arg0)

        Copy constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.Transform4f`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (numpy.ndarray):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (list):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Initialize the transformation from the given matrix (and compute its
        inverse transpose)

        Parameter ``arg0`` (drjit.llvm.ad.Matrix4f):
            *no description available*

    .. py:method:: __init__(self, arg0, arg1)

        Initialize from a matrix and its inverse transpose

        Parameter ``arg0`` (drjit.llvm.ad.Matrix4f):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Matrix4f):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Broadcast constructor

        Parameter ``arg0`` (:py:obj:`mitsuba.Transform`):
            *no description available*

    .. py:method:: mitsuba.Transform4f.assign(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Transform4f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Transform4f.extract(self)

        Extract a lower-dimensional submatrix

        Returns → :py:obj:`mitsuba.Transform3f`:
            *no description available*

    .. py:method:: mitsuba.Transform4f.from_frame(frame)

        Creates a transformation that converts from 'frame' to the standard
        basis

        Parameter ``frame`` (:py:obj:`mitsuba.Frame3f`):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<float>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4f.has_scale(overloaded)


        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → drjit.llvm.ad.Bool:
                *no description available*

        .. py:method:: has_scale(self)

            Test for a scale component in each transform matrix by checking
            whether ``M . M^T == I`` (where ``M`` is the matrix in question and
            ``I`` is the identity).

            Returns → drjit.llvm.ad.Bool:
                *no description available*

    .. py:method:: mitsuba.Transform4f.inverse(self)

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.Transform4f.look_at(origin, target, up)

        Create a look-at camera transformation

        Parameter ``origin`` (:py:obj:`mitsuba.Point3f`):
            Camera position

        Parameter ``target`` (:py:obj:`mitsuba.Point3f`):
            Target vector

        Parameter ``up`` (:py:obj:`mitsuba.Point3f`):
            Up vector

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<float>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4f.orthographic(near, far)

        Create an orthographic transformation, which maps Z to [0,1] and
        leaves the X and Y coordinates untouched.

        Parameter ``near`` (drjit.llvm.ad.Float):
            Near clipping plane

        Parameter ``far`` (drjit.llvm.ad.Float):
            Far clipping plane

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<float>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4f.perspective(fov, near, far)

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

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<float>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4f.rotate(axis, angle)

        Create a rotation transformation around an arbitrary axis in 3D. The
        angle is specified in degrees

        Parameter ``axis`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``angle`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<float>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4f.scale(v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<float>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4f.to_frame(frame)

        Creates a transformation that converts from the standard basis to
        'frame'

        Parameter ``frame`` (:py:obj:`mitsuba.Frame3f`):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<float>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4f.transform_affine(overloaded)


        .. py:method:: transform_affine(self, p)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``p`` (:py:obj:`mitsuba.Point3f`):
                *no description available*

            Returns → :py:obj:`mitsuba.Point3f`:
                *no description available*

        .. py:method:: transform_affine(self, ray)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
                *no description available*

            Returns → :py:obj:`mitsuba.Ray3f`:
                *no description available*

        .. py:method:: transform_affine(self, v)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
                *no description available*

            Returns → :py:obj:`mitsuba.Vector3f`:
                *no description available*

        .. py:method:: transform_affine(self, n)

            Transform a 3D vector/point/normal/ray by a transformation that is
            known to be an affine 3D transformation (i.e. no perspective)

            Parameter ``n`` (:py:obj:`mitsuba.Normal3f`):
                *no description available*

            Returns → :py:obj:`mitsuba.Normal3f`:
                *no description available*

    .. py:method:: mitsuba.Transform4f.translate(v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Returns → ChainTransform<drjit::DiffArray<drjit::LLVMArray<float>>, 4>:
            *no description available*

    .. py:method:: mitsuba.Transform4f.translation(self)

        Get the translation part of a matrix

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

.. py:class:: mitsuba.TransportMode

    Specifies the transport mode when sampling or evaluating a scattering
    function

    Members:

    .. py:data:: Radiance

        Radiance transport

    .. py:data:: Importance

        Importance transport

    .. py:method:: __init__(self, value)

        Parameter ``value`` (int):
            *no description available*


    .. py:method:: mitsuba.TransportMode.name
        :property:

.. py:class:: mitsuba.TraversalCallback

    Abstract class providing an interface for traversing scene graphs

    This interface can be implemented either in C++ or in Python, to be
    used in conjunction with Object::traverse() to traverse a scene graph.
    Mitsuba currently uses this mechanism to determine a scene's
    differentiable parameters.

    .. py:method:: __init__(self)


    .. py:method:: mitsuba.TraversalCallback.put_object(self, name, obj, flags)

        Inform the traversal callback that the instance references another
        Mitsuba object

        Parameter ``name`` (str):
            *no description available*

        Parameter ``obj`` (mitsuba::Object):
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

        Parameter ``flags`` (:py:obj:`mitsuba.ParamFlags`):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.UInt


    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: drjit.llvm.ad.UInt, *args) -> None

        Parameter ``arg0`` (drjit.llvm.UInt):
            *no description available*

    .. py:method:: mitsuba.UInt.abs_(self)

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.add_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.and_(overloaded)


        .. py:method:: and_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: and_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

    .. py:method:: mitsuba.UInt.andnot_(overloaded)


        .. py:method:: andnot_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: andnot_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

    .. py:method:: mitsuba.UInt.arange_

        (arg0: int, arg1: int, arg2: int) -> drjit.llvm.ad.UInt

    .. py:method:: mitsuba.UInt.assign(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt.block_sum_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.copy_(self)

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.data_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.UInt.detach_(self)

        Returns → drjit.llvm.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.detach_ref_(self)

        Returns → drjit.llvm.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.dot_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.UInt.eq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.UInt.floordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.fma_(self, arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.full_

        (arg0: int, arg1: int) -> drjit.llvm.ad.UInt

    .. py:method:: mitsuba.UInt.gather_(source, index, mask, permute=False)

        Parameter ``source`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.ge_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.UInt.gt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.UInt.iadd_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.iand_(overloaded)


        .. py:method:: iand_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: iand_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

    .. py:method:: mitsuba.UInt.ifloordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.imod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.imul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.init_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt.ior_(overloaded)


        .. py:method:: ior_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: ior_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

    .. py:method:: mitsuba.UInt.is_evaluated_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.UInt.is_literal_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.UInt.isl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.isr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.isub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.ixor_(overloaded)


        .. py:method:: ixor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: ixor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

    .. py:method:: mitsuba.UInt.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.UInt.le_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.UInt.linspace_

        (arg0: int, arg1: int, arg2: int, arg3: bool) -> drjit.llvm.ad.UInt

    .. py:method:: mitsuba.UInt.load_(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (int):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.lt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.UInt.lzcnt_(self)

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.map_(ptr, size, callback=None)

        Parameter ``ptr`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Parameter ``callback`` (Callable[[], None]):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.max_(self)

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.maximum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.migrate_(self, arg0)

        Parameter ``arg0`` (AllocType):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.min_(self)

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.minimum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.mod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.mul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.mulhi_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.neg_(self)

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.neq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.UInt.not_(self)

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.opaque_

        (arg0: int, arg1: int) -> drjit.llvm.ad.UInt

    .. py:method:: mitsuba.UInt.or_(overloaded)


        .. py:method:: or_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: or_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

    .. py:method:: mitsuba.UInt.popcnt_(self)

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.prod_(self)

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.reinterpret_array_(overloaded)


        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (:py:obj:`mitsuba.ObjectPtr`):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (:py:obj:`mitsuba.ShapePtr`):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (:py:obj:`mitsuba.MediumPtr`):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (:py:obj:`mitsuba.EmitterPtr`):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (:py:obj:`mitsuba.BSDFPtr`):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            10. reinterpret_array_(arg0: :py:obj:`mitsuba.llvm_ad_rgb.SensorPtr`) -> drjit.llvm.ad.UInt

            Parameter ``arg0`` (:py:obj:`mitsuba.PhaseFunctionPtr`):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

    .. py:method:: mitsuba.UInt.resize_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt.scatter_reduce_(self, op, target, index, mask)

        Parameter ``op`` (ReduceOp):
            *no description available*

        Parameter ``target`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt.select_

        (arg0: drjit.llvm.ad.Bool, arg1: drjit.llvm.ad.UInt, arg2: drjit.llvm.ad.UInt) -> drjit.llvm.ad.UInt

    .. py:method:: mitsuba.UInt.set_entry_(overloaded)


        .. py:method:: set_entry_(self, arg0, arg1)

            Parameter ``arg0`` (int):
                *no description available*

            Parameter ``arg1`` (int):
                *no description available*

        .. py:method:: set_entry_(self, arg0, arg1)

            Parameter ``arg0`` (int):
                *no description available*

            Parameter ``arg1`` (int):
                *no description available*

    .. py:method:: mitsuba.UInt.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt.set_index_ad_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt.sl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.sr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.sub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.sum_(self)

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.tzcnt_(self)

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.UInt.xor_(overloaded)


        .. py:method:: xor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

        .. py:method:: xor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt:
                *no description available*

    .. py:method:: mitsuba.UInt.zero_

        (arg0: int) -> drjit.llvm.ad.UInt

.. py:class:: mitsuba.UInt64


    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Int64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.Float64):
            *no description available*

    .. py:method:: __init__(self, arg0)

        10. __init__(self: drjit.llvm.ad.UInt64, *args) -> None

        Parameter ``arg0`` (drjit.llvm.UInt64):
            *no description available*

    .. py:method:: mitsuba.UInt64.abs_(self)

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.add_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.and_(overloaded)


        .. py:method:: and_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt64):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

        .. py:method:: and_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

    .. py:method:: mitsuba.UInt64.andnot_(overloaded)


        .. py:method:: andnot_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt64):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

        .. py:method:: andnot_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

    .. py:method:: mitsuba.UInt64.arange_

        (arg0: int, arg1: int, arg2: int) -> drjit.llvm.ad.UInt64

    .. py:method:: mitsuba.UInt64.assign(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt64.block_sum_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.copy_(self)

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.data_(self)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.UInt64.detach_(self)

        Returns → drjit.llvm.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.detach_ref_(self)

        Returns → drjit.llvm.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.dot_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.entry_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.UInt64.eq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.UInt64.floordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.fma_(self, arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.full_

        (arg0: int, arg1: int) -> drjit.llvm.ad.UInt64

    .. py:method:: mitsuba.UInt64.gather_(source, index, mask, permute=False)

        Parameter ``source`` (drjit.llvm.ad.UInt64):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.ge_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.UInt64.gt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.UInt64.iadd_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.iand_(overloaded)


        .. py:method:: iand_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt64):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

        .. py:method:: iand_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

    .. py:method:: mitsuba.UInt64.ifloordiv_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.imod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.imul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.init_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt64.ior_(overloaded)


        .. py:method:: ior_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt64):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

        .. py:method:: ior_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

    .. py:method:: mitsuba.UInt64.is_evaluated_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.UInt64.is_literal_(self)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.UInt64.isl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.isr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.isub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.ixor_(overloaded)


        .. py:method:: ixor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt64):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

        .. py:method:: ixor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

    .. py:method:: mitsuba.UInt64.label_(self)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.UInt64.le_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.UInt64.linspace_

        (arg0: int, arg1: int, arg2: int, arg3: bool) -> drjit.llvm.ad.UInt64

    .. py:method:: mitsuba.UInt64.load_(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (int):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.lt_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.UInt64.lzcnt_(self)

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.map_(ptr, size, callback=None)

        Parameter ``ptr`` (int):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Parameter ``callback`` (Callable[[], None]):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.max_(self)

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.maximum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.migrate_(self, arg0)

        Parameter ``arg0`` (AllocType):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.min_(self)

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.minimum_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.mod_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.mul_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.mulhi_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.neg_(self)

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.neq_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.UInt64.not_(self)

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.opaque_

        (arg0: int, arg1: int) -> drjit.llvm.ad.UInt64

    .. py:method:: mitsuba.UInt64.or_(overloaded)


        .. py:method:: or_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt64):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

        .. py:method:: or_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

    .. py:method:: mitsuba.UInt64.popcnt_(self)

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.prod_(self)

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.reinterpret_array_(overloaded)


        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Int64):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt64):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

        .. py:method:: reinterpret_array_(arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Float64):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

    .. py:method:: mitsuba.UInt64.resize_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt64.scatter_(self, target, index, mask, permute=False)

        Parameter ``target`` (drjit.llvm.ad.UInt64):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Parameter ``permute`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt64.scatter_reduce_(self, op, target, index, mask)

        Parameter ``op`` (ReduceOp):
            *no description available*

        Parameter ``target`` (drjit.llvm.ad.UInt64):
            *no description available*

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt64.select_

        (arg0: drjit.llvm.ad.Bool, arg1: drjit.llvm.ad.UInt64, arg2: drjit.llvm.ad.UInt64) -> drjit.llvm.ad.UInt64

    .. py:method:: mitsuba.UInt64.set_entry_(overloaded)


        .. py:method:: set_entry_(self, arg0, arg1)

            Parameter ``arg0`` (int):
                *no description available*

            Parameter ``arg1`` (int):
                *no description available*

        .. py:method:: set_entry_(self, arg0, arg1)

            Parameter ``arg0`` (int):
                *no description available*

            Parameter ``arg1`` (int):
                *no description available*

    .. py:method:: mitsuba.UInt64.set_index_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt64.set_index_ad_(self, arg0)

        Parameter ``arg0`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt64.set_label_(self, arg0)

        Parameter ``arg0`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.UInt64.sl_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.sr_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.sub_(self, arg0)

        Parameter ``arg0`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.sum_(self)

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.tzcnt_(self)

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.UInt64.xor_(overloaded)


        .. py:method:: xor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.UInt64):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

        .. py:method:: xor_(self, arg0)

            Parameter ``arg0`` (drjit.llvm.ad.Bool):
                *no description available*

            Returns → drjit.llvm.ad.UInt64:
                *no description available*

    .. py:method:: mitsuba.UInt64.zero_

        (arg0: int) -> drjit.llvm.ad.UInt64

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


    .. py:method:: mitsuba.Volume.bbox(self)

        Returns the bounding box of the volume

        Returns → :py:obj:`mitsuba.ScalarBoundingBox3f`:
            *no description available*

    .. py:method:: mitsuba.Volume.channel_count(self)

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

        Returns → List[drjit.llvm.ad.Float[6]]:
            *no description available*

    .. py:method:: mitsuba.Volume.eval_gradient(self, it, active=True)

        Evaluate the volume at the given surface interaction, and compute the
        gradients of the linear interpolant as well.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → Tuple[:py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Vector3f`]:
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

        Returns → List[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Volume.max(self)

        Returns the maximum value of the volume over all dimensions.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.Volume.max_per_channel(self)

        In the case of a multi-channel volume, this function returns the
        maximum value for each channel.

        Pointer allocation/deallocation must be performed by the caller.

        Returns → List[float]:
            *no description available*

    .. py:method:: mitsuba.Volume.resolution(self)

        Returns the resolution of the volume, assuming that it is based on a
        discrete representation.

        The default implementation returns ``(1, 1, 1)``

        Returns → :py:obj:`mitsuba.ScalarVector3i`:
            *no description available*

.. py:class:: mitsuba.VolumeGrid

    Base class: :py:obj:`mitsuba.Object`

    Overloaded function.

    1. __init__(self: :py:obj:`mitsuba.llvm_ad_rgb.VolumeGrid`, array: numpy.ndarray[numpy.float32], compute_max: bool = True) -> None

    Initialize a VolumeGrid from a NumPy array

    2. __init__(self: :py:obj:`mitsuba.llvm_ad_rgb.VolumeGrid`, path: :py:obj:`mitsuba.filesystem.path`) -> None

    3. __init__(self: :py:obj:`mitsuba.llvm_ad_rgb.VolumeGrid`, stream: :py:obj:`mitsuba.Stream`) -> None

    .. py:method:: mitsuba.VolumeGrid.buffer_size(self)

        Return the volume grid size in bytes (excluding metadata)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.bytes_per_voxel(self)

        Return the number bytes of storage used per voxel

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.channel_count(self)

        Return the number of channels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.max(self)

        Return the precomputed maximum over the volume grid

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.max_per_channel(self)

        Return the precomputed maximum over the volume grid per channel

        Pointer allocation/deallocation must be performed by the caller.

        Returns → List[float]:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.set_max(self, arg0)

        Set the precomputed maximum over the volume grid

        Parameter ``arg0`` (float):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.set_max_per_channel(self, arg0)

        Set the precomputed maximum over the volume grid per channel

        Pointer allocation/deallocation must be performed by the caller.

        Parameter ``arg0`` (List[float]):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.size(self)

        Return the resolution of the voxel grid

        Returns → :py:obj:`mitsuba.ScalarVector3u`:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.write(overloaded)


        .. py:method:: write(self, stream)

            Write an encoded form of the bitmap to a binary volume file

            Parameter ``path`` (:py:obj:`mitsuba.filesystem.path`):
                Target file name (expected to end in ".vol")

            Parameter ``stream`` (:py:obj:`mitsuba.Stream`):
                *no description available*

        .. py:method:: write(self, path)

            Write an encoded form of the volume grid to a stream

            Parameter ``stream``:
                Target stream that will receive the encoded output

.. py:class:: mitsuba.ZStream

    Base class: :py:obj:`mitsuba.Stream`

    Transparent compression/decompression stream based on ``zlib``.

    This class transparently decompresses and compresses reads and writes
    to a nested stream, respectively.

    .. py:method:: __init__(self, child_stream, stream_type=<EStreamType., EDeflateStream, level=-1)

        Creates a new compression stream with the given underlying stream.
        This new instance takes ownership of the child stream. The child
        stream must outlive the ZStream.

        Parameter ``child_stream`` (:py:obj:`mitsuba.Stream`):
            *no description available*

        Parameter ``stream_type`` (:py:obj:`mitsuba.ZStream.EStreamType`):
            *no description available*

        Parameter ``EDeflateStream`` (0>):
            *no description available*

        Parameter ``level`` (int):
            *no description available*


    .. py:class:: mitsuba.ZStream.EStreamType

        Members:

        .. py:data:: EDeflateStream



        .. py:data:: EGZipStream

            A raw deflate stream

        .. py:method:: __init__(self, value)

            Parameter ``value`` (int):
                *no description available*


    .. py:method:: mitsuba.ZStream.EStreamType.name
        :property:

    .. py:method:: mitsuba.ZStream.child_stream(self)

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

.. py:class:: mitsuba.ad.common.ADIntegrator

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

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Properties`):
            *no description available*


    .. py:method:: mitsuba.ad.common.ADIntegrator.render(overloaded)


        .. py:method:: render(self, scene, sensor, seed=0, spp=0, develop=True, evaluate=True)

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

            Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
                *no description available*

            Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
                *no description available*

            Returns → drjit.llvm.ad.TensorXf:
                *no description available*

        .. py:method:: render(self, scene, sensor=0, seed=0, spp=0, develop=True, evaluate=True)

            Render the scene

            This function is just a thin wrapper around the previous render()
            overload. It accepts a sensor *index* instead and renders the scene
            using sensor 0 by default.

            Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
                *no description available*

            Parameter ``sensor`` (int):
                *no description available*

            Parameter ``seed`` (int):
                *no description available*

            Parameter ``spp`` (int):
                *no description available*

            Parameter ``develop`` (bool):
                *no description available*

            Parameter ``evaluate`` (bool):
                *no description available*

            Returns → drjit.llvm.ad.TensorXf:
                *no description available*

    .. py:method:: mitsuba.ad.common.ADIntegrator.render_forward(scene, params, sensor=0, seed=0, spp=0)

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
        that can be obtained obtained via a call to ``mi.traverse()``.

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
            The scene to be rendered differentially.

        Parameter ``params`` (Any):
           An arbitrary container of scene parameters that should receive
           gradients. Typically this will be an instance of type
           ``mi.SceneParameters`` obtained via ``mi.traverse()``. However, it could
           also be a Python list/dict/object tree (DrJit will traverse it to find
           all parameters). Gradient tracking must be explicitly enabled for each of
           these parameters using ``dr.enable_grad(params['parameter_name'])`` (i.e.
           ``render_forward()`` will not do this for you). Furthermore,
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

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → mi.TensorXf:
            *no description available*

    .. py:method:: mitsuba.ad.common.ADIntegrator.render_backward(scene, params, grad_in, sensor=0, seed=0, spp=0)

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
        to ``mi.traverse()``. Use ``dr.grad()`` to query the resulting gradients of
        these parameters once ``render_backward()`` returns.

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
            The scene to be rendered differentially.

        Parameter ``params`` (Any):
           An arbitrary container of scene parameters that should receive
           gradients. Typically this will be an instance of type
           ``mi.SceneParameters`` obtained via ``mi.traverse()``. However, it could
           also be a Python list/dict/object tree (DrJit will traverse it to find
           all parameters). Gradient tracking must be explicitly enabled for each of
           these parameters using ``dr.enable_grad(params['parameter_name'])`` (i.e.
           ``render_backward()`` will not do this for you).

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

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ad.common.ADIntegrator.sample_rays(scene, sensor, sampler, reparam=None)

        Sample a 2D grid of primary rays for a given sensor

        Returns a tuple containing

        - the set of sampled rays
        - a ray weight (usually 1 if the sensor's response function is sampled
          perfectly)
        - the continuous 2D image-space positions associated with each ray

        When a reparameterization function is provided via the 'reparam'
        argument, it will be applied to the returned image-space position (i.e.
        the sample positions will be moving). The other two return values
        remain detached.

        Parameter ``scene`` (mi.Scene):
            *no description available*

        Parameter ``sensor`` (mi.Sensor):
            *no description available*

        Parameter ``sampler`` (mi.Sampler):
            *no description available*

        Parameter ``reparam`` (Callable[[mi.Ray3f, mi.UInt32, mi.Bool], Tuple[mi.Vector3f, mi.Float]]):
            *no description available*

        Returns → Tuple[mi.RayDifferential3f, mi.Spectrum, mi.Vector2f, mi.Float]:
            *no description available*

    .. py:method:: mitsuba.ad.common.ADIntegrator.prepare(sensor, seed=0, spp=0, aovs=[])

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

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Parameter ``aovs`` (list):
            *no description available*

    .. py:method:: mitsuba.ad.common.ADIntegrator.sample(mode, scene, sampler, ray, depth, L, state_in, reparam, active)

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

        Parameter ``reparam`` (see above):
            If provided, this callable takes a ray and a mask of active SIMD
            lanes and returns a reparameterized ray and Jacobian determinant.
            The implementation of the ``sample`` function should then use it to
            correctly account for visibility-induced discontinuities during
            differentiation.

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

        Parameter ``L`` (Optional[mi.Spectrum]):
            *no description available*

        Parameter ``state_in`` (Any):
            *no description available*

        Parameter ``reparam`` (Optional[Callable[[mi.Ray3f, mi.UInt32, mi.Bool], Tuple[mi.Vector3f, mi.Float]]]):
            *no description available*

        Parameter ``active`` (mi.Bool):
            Mask to specify active lanes.

        Returns → Tuple[mi.Spectrum, mi.Bool]:
            *no description available*

.. py:class:: mitsuba.ad.common.RBIntegrator

    Base class: :py:obj:`mitsuba.ad.integrators.common.ADIntegrator`

    Abstract base class of radiative-backpropagation style differentiable
    integrators.

    .. py:method:: __init__(self, arg0)

        Parameter ``arg0`` (:py:obj:`mitsuba.Properties`):
            *no description available*


    .. py:method:: mitsuba.ad.common.RBIntegrator.render_forward(scene, params, sensor=0, seed=0, spp=0)

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

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → mi.TensorXf:
            *no description available*

    .. py:method:: mitsuba.ad.common.RBIntegrator.render_backward(scene, params, grad_in, sensor=0, seed=0, spp=0)

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

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → None:
            *no description available*

.. py:function:: mitsuba.ad.common.mis_weight()

    Compute the Multiple Importance Sampling (MIS) weight given the densities
    of two sampling strategies according to the power heuristic.

.. py:class:: mitsuba.ad.largesteps.SolveCholesky

    DrJIT custom operator to solve a linear system using a Cholesky factorization.

    .. py:method:: mitsuba.ad.largesteps.SolveCholesky.eval()

        Evaluate the custom function in primal mode.

        The inputs will be detached from the AD graph, and the output *must* also be
        detached.

        .. danger::

            This method must be overriden, no default implementation provided.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.ad.largesteps.SolveCholesky.forward()

        Evaluated forward-mode derivatives.

        .. danger::

            This method must be overriden, no default implementation provided.

    .. py:method:: mitsuba.ad.largesteps.SolveCholesky.backward()

        Evaluated backward-mode derivatives.

        .. danger::

            This method must be overriden, no default implementation provided.

    .. py:method:: mitsuba.ad.largesteps.SolveCholesky.name()

        Return a descriptive name of the ``CustomOp`` instance.

        The name returned by this method is used in the GraphViz output.

        If not overriden, this method returns ``"CustomOp[unnamed]"``.

.. py:function:: mitsuba.ad.largesteps.mesh_laplacian()

    Compute the index and data arrays of the (combinatorial) Laplacian matrix of
    a given mesh.

.. py:function:: mitsuba.ad.reparameterize_ray(scene, rng, params, ray, num_rays=4, kappa=100000.0, exponent=3.0, antithetic=False, unroll=False, active=True)

    Reparameterize given ray by "attaching" the derivatives of its direction to
    moving geometry in the scene.

    Parameter ``scene`` (``mitsuba.Scene``):
        Scene containing all shapes.

    Parameter ``rng`` (``mitsuba.PCG32``):
        Random number generator used to sample auxiliary ray directions.

    Parameter ``params`` (``mitsuba.SceneParameters``):
        Scene parameters

    Parameter ``ray`` (``mitsuba.RayDifferential3f``):
        Ray to be reparameterized

    Parameter ``num_rays`` (``int``):
        Number of auxiliary rays to trace when performing the convolution.

    Parameter ``kappa`` (``float``):
        Kappa parameter of the von Mises Fisher distribution used to sample the
        auxiliary rays.

    Parameter ``exponent`` (``float``):
        Exponent used in the computation of the harmonic weights

    Parameter ``antithetic`` (``bool``):
        Should antithetic sampling be enabled to improve convergence?
        (Default: False)

    Parameter ``unroll`` (``bool``):
        Should the loop tracing auxiliary rays be unrolled? (Default: False)

    Parameter ``active`` (``mitsuba.Bool``):
        Boolean array specifying the active lanes

    Returns → (:py:obj:`mitsuba.Vector3f`, :py:obj:`mitsuba.Float`):
        Returns the reparameterized ray direction and the Jacobian
        determinant of the change of variables.

    Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
        *no description available*

    Parameter ``rng`` (:py:obj:`mitsuba.PCG32`):
        *no description available*

    Parameter ``params`` (:py:obj:`mitsuba.SceneParameters`):
        *no description available*

    Parameter ``ray`` (:py:obj:`mitsuba.RayDifferential3f`):
        *no description available*

    Parameter ``num_rays`` (int):
        *no description available*

    Parameter ``kappa`` (float):
        *no description available*

    Parameter ``exponent`` (float):
        *no description available*

    Parameter ``antithetic`` (bool):
        *no description available*

    Parameter ``unroll`` (bool):
        *no description available*

    Parameter ``active`` (:py:obj:`mitsuba.Bool`):
        Mask to specify active lanes.

    Returns → Tuple[:py:obj:`mitsuba.Vector3f`, :py:obj:`mitsuba.Float`]:
        *no description available*

.. py:function:: mitsuba.chi2.BSDFAdapter()

    Adapter to test BSDF sampling using the Chi^2 test.

    Parameter ``bsdf_type`` (string):
        Name of the BSDF plugin to instantiate.

    Parameter ``extra`` (string):
        Additional XML used to specify the BSDF's parameters.

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

    Parameter ``extra`` (string):
        Additional XML used to specify the emitter's parameters.

.. py:class:: mitsuba.chi2.LineDomain

    The identity map on the line.

.. py:function:: mitsuba.chi2.MicrofacetAdapter()

    Adapter for testing microfacet distribution sampling techniques
    (separately from BSDF models, which are also tested)

.. py:function:: mitsuba.chi2.PhaseFunctionAdapter()

    Adapter to test phase function sampling using the Chi^2 test.

    Parameter ``phase_type`` (string):
        Name of the phase function plugin to instantiate.

    Parameter ``extra`` (string):
        Additional XML used to specify the phase function's parameters.

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

    Returns → Tuple[:py:obj:`mitsuba.Vector3f`, :py:obj:`mitsuba.Vector3f`]:
        *no description available*

.. py:function:: mitsuba.cornell_box()

    Returns a dictionary containing a description of the Cornell Box scene.

.. py:function:: mitsuba.depolarizer(arg0)

    Parameter ``arg0`` (:py:obj:`mitsuba.Color3f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Color3f`:
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

.. py:function:: mitsuba.filesystem.absolute(arg0)

    Returns an absolute path to the same location pointed by ``p``,
    relative to ``base``.

    See also:
        http ://en.cppreference.com/w/cpp/experimental/fs/absolute)

    Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
        *no description available*

    Returns → :py:obj:`mitsuba.filesystem.path`:
        *no description available*

.. py:function:: mitsuba.filesystem.create_directory(arg0)

    Creates a directory at ``p`` as if ``mkdir`` was used. Returns true if
    directory creation was successful, false otherwise. If ``p`` already
    exists and is already a directory, the function does nothing (this
    condition is not treated as an error).

    Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
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

    Parameter ``arg1`` (:py:obj:`mitsuba.filesystem.path`):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.filesystem.exists(arg0)

    Checks if ``p`` points to an existing filesystem object.

    Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.filesystem.file_size(arg0)

    Returns the size (in bytes) of a regular file at ``p``. Attempting to
    determine the size of a directory (as well as any other file that is
    not a regular file or a symlink) is treated as an error.

    Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
        *no description available*

    Returns → int:
        *no description available*

.. py:function:: mitsuba.filesystem.is_directory(arg0)

    Checks if ``p`` points to a directory.

    Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.filesystem.is_regular_file(arg0)

    Checks if ``p`` points to a regular file, as opposed to a directory or
    symlink.

    Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
        *no description available*

    Returns → bool:
        *no description available*

.. py:class:: mitsuba.filesystem.path

    Represents a path to a filesystem resource. On construction, the path
    is parsed and stored in a system-agnostic representation. The path can
    be converted back to the system-specific string using ``native()`` or
    ``string()``.


    .. py:method:: __init__(self)

        Default constructor. Constructs an empty path. An empty path is
        considered relative.

    .. py:method:: __init__(self, arg0)

        Copy constructor.

        Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

    .. py:method:: __init__(self, arg0)

        Construct a path from a string with native type. On Windows, the path
        can use both '/' or '\\' as a delimiter.

        Parameter ``arg0`` (str):
            *no description available*

    .. py:method:: mitsuba.filesystem.path.clear(self)

        Makes the path an empty path. An empty path is considered relative.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.empty(self)

        Checks if the path is empty

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.extension(self)

        Returns the extension of the filename component of the path (the
        substring starting at the rightmost period, including the period).
        Special paths '.' and '..' have an empty extension.

        Returns → :py:obj:`mitsuba.filesystem.path`:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.filename(self)

        Returns the filename component of the path, including the extension.

        Returns → :py:obj:`mitsuba.filesystem.path`:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.is_absolute(self)

        Checks if the path is absolute.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.is_relative(self)

        Checks if the path is relative.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.native(self)

        Returns the path in the form of a native string, so that it can be
        passed directly to system APIs. The path is constructed using the
        system's preferred separator and the native string type.

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.parent_path(self)

        Returns the path to the parent directory. Returns an empty path if it
        is already empty or if it has only one element.

        Returns → :py:obj:`mitsuba.filesystem.path`:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.replace_extension(self, arg0)

        Replaces the substring starting at the rightmost '.' symbol by the
        provided string.

        A '.' symbol is automatically inserted if the replacement does not
        start with a dot. Removes the extension altogether if the empty path
        is passed. If there is no extension, appends a '.' followed by the
        replacement. If the path is empty, '.' or '..', the method does
        nothing.

        Returns *this.

        Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Returns → :py:obj:`mitsuba.filesystem.path`:
            *no description available*

.. py:data:: mitsuba.filesystem.preferred_separator
    :type: str
    :value: /

.. py:function:: mitsuba.filesystem.remove(arg0)

    Removes a file or empty directory. Returns true if removal was
    successful, false if there was an error (e.g. the file did not exist).

    Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.filesystem.resize_file(arg0, arg1)

    Changes the size of the regular file named by ``p`` as if ``truncate``
    was called. If the file was larger than ``target_length``, the
    remainder is discarded. The file must exist.

    Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
        *no description available*

    Parameter ``arg1`` (int):
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

    Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
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

    Returns → Tuple[drjit.llvm.ad.Complex2f, drjit.llvm.ad.Complex2f, drjit.llvm.ad.Float, drjit.llvm.ad.Complex2f, drjit.llvm.ad.Complex2f]:
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

.. py:function:: mitsuba.has_flag(overloaded)


    .. py:function:: has_flag(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.EmitterFlags`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:function:: has_flag(arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.EmitterFlags`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:function:: has_flag(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.RayFlags`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:function:: has_flag(arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.RayFlags`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:function:: has_flag(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.BSDFFlags`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:function:: has_flag(arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.BSDFFlags`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:function:: has_flag(arg0, arg1)

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.FilmFlags`):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:function:: has_flag(arg0, arg1)

        Parameter ``arg0`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.FilmFlags`):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:function:: has_flag(arg0, arg1)

        10. has_flag(arg0: drjit.llvm.ad.UInt, arg1: :py:obj:`mitsuba.PhaseFunctionFlags`) -> bool

        Parameter ``arg0`` (int):
            *no description available*

        Parameter ``arg1`` (:py:obj:`mitsuba.PhaseFunctionFlags`):
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

.. py:function:: mitsuba.luminance(overloaded)


    .. py:function:: luminance(value, wavelengths, active=True)

        Parameter ``value`` (:py:obj:`mitsuba.Color3f`):
            *no description available*

        Parameter ``wavelengths`` (:py:obj:`mitsuba.Color0f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:function:: luminance(c)

        Parameter ``c`` (:py:obj:`mitsuba.Color3f`):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

.. py:data:: mitsuba.math.RayEpsilon
    :type: float
    :value: 8.940696716308594e-05

.. py:data:: mitsuba.math.ShadowEpsilon
    :type: float
    :value: 0.0008940696716308594

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

    Parameter ``arg0`` (drjit.scalar.ArrayXf64):
        *no description available*

    Parameter ``arg1`` (drjit.scalar.ArrayXf64):
        *no description available*

    Parameter ``arg2`` (float):
        *no description available*

    Returns → Tuple[float, int, int, int]:
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

    Parameter ``pred`` (Callable[[drjit.llvm.ad.UInt], drjit.llvm.ad.Bool]):
        *no description available*

    Returns → drjit.llvm.ad.UInt:
        *no description available*

.. py:function:: mitsuba.math.is_power_of_two(arg0)

    Check whether the provided integer is a power of two

    Parameter ``arg0`` (int):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.math.legendre_p(overloaded)


    .. py:function:: legendre_p(l, x)

        Evaluate the l-th Legendre polynomial using recurrence

        Parameter ``l`` (int):
            *no description available*

        Parameter ``x`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:function:: legendre_p(l, m, x)

        Evaluate the l-th Legendre polynomial using recurrence

        Parameter ``l`` (int):
            *no description available*

        Parameter ``m`` (int):
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

    Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        *no description available*

.. py:function:: mitsuba.math.legendre_pd_diff(l, x)

    Evaluate the function legendre_pd(l+1, x) - legendre_pd(l-1, x)

    Parameter ``l`` (int):
        *no description available*

    Parameter ``x`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        *no description available*

.. py:function:: mitsuba.math.linear_to_srgb(arg0)

    Applies the sRGB gamma curve to the given argument.

    Parameter ``arg0`` (drjit.llvm.ad.Float):
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

.. py:function:: mitsuba.math.rlgamma()

    Regularized lower incomplete gamma function based on CEPHES

.. py:function:: mitsuba.math.round_to_power_of_two(arg0)

    Round an unsigned integer to the next integer power of two

    Parameter ``arg0`` (int):
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

    Returns → Tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        ``True`` if a solution could be found

.. py:function:: mitsuba.math.srgb_to_linear(arg0)

    Applies the inverse sRGB gamma curve to the given argument.

    Parameter ``arg0`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.math.ulpdiff(arg0, arg1)

    Compare the difference in ULPs between a reference value and another
    given floating point number

    Parameter ``arg0`` (float):
        *no description available*

    Parameter ``arg1`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.mueller.absorber(overloaded)


    .. py:function:: absorber(value)

        Constructs the Mueller matrix of an ideal absorber

        Parameter ``value`` (drjit.llvm.ad.Float):
            The amount of absorption.

        Returns → drjit.llvm.ad.Matrix4f:
            *no description available*

    .. py:function:: absorber(value)

        Constructs the Mueller matrix of an ideal absorber

        Parameter ``value`` (:py:obj:`mitsuba.Color3f`):
            The amount of absorption.

        Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
            *no description available*

.. py:function:: mitsuba.mueller.depolarizer(overloaded)


    .. py:function:: depolarizer(value=1.0)

        Constructs the Mueller matrix of an ideal depolarizer

        Parameter ``value`` (drjit.llvm.ad.Float):
            The value of the (0, 0) element

        Returns → drjit.llvm.ad.Matrix4f:
            *no description available*

    .. py:function:: depolarizer(value=1.0)

        Constructs the Mueller matrix of an ideal depolarizer

        Parameter ``value`` (:py:obj:`mitsuba.Color3f`):
            The value of the (0, 0) element

        Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
            *no description available*

.. py:function:: mitsuba.mueller.diattenuator(overloaded)


    .. py:function:: diattenuator(x, y)

        Constructs the Mueller matrix of a linear diattenuator, which
        attenuates the electric field components at 0 and 90 degrees by 'x'
        and 'y', * respectively.

        Parameter ``x`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``y`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Matrix4f:
            *no description available*

    .. py:function:: diattenuator(x, y)

        Constructs the Mueller matrix of a linear diattenuator, which
        attenuates the electric field components at 0 and 90 degrees by 'x'
        and 'y', * respectively.

        Parameter ``x`` (:py:obj:`mitsuba.Color3f`):
            *no description available*

        Parameter ``y`` (:py:obj:`mitsuba.Color3f`):
            *no description available*

        Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
            *no description available*

.. py:function:: mitsuba.mueller.left_circular_polarizer(overloaded)


    .. py:function:: left_circular_polarizer()

        Constructs the Mueller matrix of a (left) circular polarizer.

        "Polarized Light and Optical Systems" by Chipman et al. Table 6.2

        Returns → drjit.llvm.ad.Matrix4f:
            *no description available*

    .. py:function:: left_circular_polarizer()

        Constructs the Mueller matrix of a (left) circular polarizer.

        "Polarized Light and Optical Systems" by Chipman et al. Table 6.2

        Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
            *no description available*

.. py:function:: mitsuba.mueller.linear_polarizer(overloaded)


    .. py:function:: linear_polarizer(value=1.0)

        Constructs the Mueller matrix of a linear polarizer which transmits
        linear polarization at 0 degrees.

        "Polarized Light" by Edward Collett, Ch. 5 eq. (13)

        Parameter ``value`` (drjit.llvm.ad.Float):
            The amount of attenuation of the transmitted component (1
            corresponds to an ideal polarizer).

        Returns → drjit.llvm.ad.Matrix4f:
            *no description available*

    .. py:function:: linear_polarizer(value=1.0)

        Constructs the Mueller matrix of a linear polarizer which transmits
        linear polarization at 0 degrees.

        "Polarized Light" by Edward Collett, Ch. 5 eq. (13)

        Parameter ``value`` (:py:obj:`mitsuba.Color3f`):
            The amount of attenuation of the transmitted component (1
            corresponds to an ideal polarizer).

        Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
            *no description available*

.. py:function:: mitsuba.mueller.linear_retarder(overloaded)


    .. py:function:: linear_retarder(phase)

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

    .. py:function:: linear_retarder(phase)

        Constructs the Mueller matrix of a linear retarder which has its fast
        axis aligned horizontally.

        This implements the general case with arbitrary phase shift and can be
        used to construct the common special cases of quarter-wave and half-
        wave plates.

        "Polarized Light, Third Edition" by Dennis H. Goldstein, Ch. 6 eq.
        (6.43) (Note that the fast and slow axis were flipped in the first
        edition by Edward Collett.)

        Parameter ``phase`` (:py:obj:`mitsuba.Color3f`):
            The phase difference between the fast and slow axis

        Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
            *no description available*

.. py:function:: mitsuba.mueller.right_circular_polarizer(overloaded)


    .. py:function:: right_circular_polarizer()

        Constructs the Mueller matrix of a (right) circular polarizer.

        "Polarized Light and Optical Systems" by Chipman et al. Table 6.2

        Returns → drjit.llvm.ad.Matrix4f:
            *no description available*

    .. py:function:: right_circular_polarizer()

        Constructs the Mueller matrix of a (right) circular polarizer.

        "Polarized Light and Optical Systems" by Chipman et al. Table 6.2

        Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
            *no description available*

.. py:function:: mitsuba.mueller.rotate_mueller_basis(overloaded)


    .. py:function:: rotate_mueller_basis(M, in_forward, in_basis_current, in_basis_target, out_forward, out_basis_current, out_basis_target)

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
            Current (normalized) input Stokes basis. Must be orthogonal to
            ``out_forward``.

        Parameter ``out_basis_target`` (:py:obj:`mitsuba.Vector3f`):
            Target (normalized) input Stokes basis. Must be orthogonal to
            ``out_forward``.

        Returns → drjit.llvm.ad.Matrix4f:
            New Mueller matrix that operates from ``in_basis_target`` to
            ``out_basis_target``.

    .. py:function:: rotate_mueller_basis(M, in_forward, in_basis_current, in_basis_target, out_forward, out_basis_current, out_basis_target)

        Return the Mueller matrix for some new reference frames. This version
        rotates the input/output frames independently.

        This operation is often used in polarized light transport when we have
        a known Mueller matrix 'M' that operates from 'in_basis_current' to
        'out_basis_current' but instead want to re-express it as a Mueller
        matrix that operates from 'in_basis_target' to 'out_basis_target'.

        Parameter ``M`` (drjit::Matrix<:py:obj:`mitsuba.Color`):
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
            Current (normalized) input Stokes basis. Must be orthogonal to
            ``out_forward``.

        Parameter ``out_basis_target`` (:py:obj:`mitsuba.Vector3f`):
            Target (normalized) input Stokes basis. Must be orthogonal to
            ``out_forward``.

        Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
            New Mueller matrix that operates from ``in_basis_target`` to
            ``out_basis_target``.

.. py:function:: mitsuba.mueller.rotate_mueller_basis_collinear(overloaded)


    .. py:function:: rotate_mueller_basis_collinear(M, forward, basis_current, basis_target)

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

    .. py:function:: rotate_mueller_basis_collinear(M, forward, basis_current, basis_target)

        Return the Mueller matrix for some new reference frames. This version
        applies the same rotation to the input/output frames.

        This operation is often used in polarized light transport when we have
        a known Mueller matrix 'M' that operates from 'basis_current' to
        'basis_current' but instead want to re-express it as a Mueller matrix
        that operates from 'basis_target' to 'basis_target'.

        Parameter ``M`` (drjit::Matrix<:py:obj:`mitsuba.Color`):
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

        Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
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

    Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
        Mueller matrix that performs the desired change of reference
        frames.

.. py:function:: mitsuba.mueller.rotated_element(overloaded)


    .. py:function:: rotated_element(theta, M)

        Applies a counter-clockwise rotation to the mueller matrix of a given
        element.

        Parameter ``theta`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``M`` (drjit.llvm.ad.Matrix4f):
            *no description available*

        Returns → drjit.llvm.ad.Matrix4f:
            *no description available*

    .. py:function:: rotated_element(theta, M)

        Applies a counter-clockwise rotation to the mueller matrix of a given
        element.

        Parameter ``theta`` (:py:obj:`mitsuba.Color3f`):
            *no description available*

        Parameter ``M`` (drjit::Matrix<:py:obj:`mitsuba.Color`):
            *no description available*

        Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
            *no description available*

.. py:function:: mitsuba.mueller.rotator(overloaded)


    .. py:function:: rotator(theta)

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

    .. py:function:: rotator(theta)

        Constructs the Mueller matrix of an ideal rotator, which performs a
        counter-clockwise rotation of the electric field by 'theta' radians
        (when facing the light beam from the sensor side).

        To be more precise, it rotates the reference frame of the current
        Stokes vector. For example: horizontally linear polarized light s1 =
        [1,1,0,0] will look like -45˚ linear polarized light s2 = R(45˚) * s1
        = [1,0,-1,0] after applying a rotator of +45˚ to it.

        "Polarized Light" by Edward Collett, Ch. 5 eq. (43)

        Parameter ``theta`` (:py:obj:`mitsuba.Color3f`):
            *no description available*

        Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
            *no description available*

.. py:function:: mitsuba.mueller.specular_reflection(overloaded)


    .. py:function:: specular_reflection(cos_theta_i, eta)

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

    .. py:function:: specular_reflection(cos_theta_i, eta)

        Calculates the Mueller matrix of a specular reflection at an interface
        between two dielectrics or conductors.

        Parameter ``cos_theta_i`` (:py:obj:`mitsuba.Color3f`):
            Cosine of the angle between the surface normal and the incident
            ray

        Parameter ``eta`` (drjit::Complex<:py:obj:`mitsuba.Color`):
            Complex-valued relative refractive index of the interface. In the
            real case, a value greater than 1.0 case means that the surface
            normal points into the region of lower density.

        Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
            *no description available*

.. py:function:: mitsuba.mueller.specular_transmission(overloaded)


    .. py:function:: specular_transmission(cos_theta_i, eta)

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

    .. py:function:: specular_transmission(cos_theta_i, eta)

        Calculates the Mueller matrix of a specular transmission at an
        interface between two dielectrics or conductors.

        Parameter ``cos_theta_i`` (:py:obj:`mitsuba.Color3f`):
            Cosine of the angle between the surface normal and the incident
            ray

        Parameter ``eta`` (:py:obj:`mitsuba.Color3f`):
            Complex-valued relative refractive index of the interface. A value
            greater than 1.0 in the real case means that the surface normal is
            pointing into the region of lower density.

        Returns → drjit::Matrix<:py:obj:`mitsuba.Color`:
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

.. py:function:: mitsuba.parse_fov(props, aspect)

    Helper function to parse the field of view field of a camera

    Parameter ``props`` (mitsuba::Properties):
        *no description available*

    Parameter ``aspect`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.pdf_rgb_spectrum(overloaded)


    .. py:function:: pdf_rgb_spectrum(wavelengths)

        PDF for the sample_rgb_spectrum strategy. It is valid to call this
        function for a single wavelength (Float), a set of wavelengths
        (Spectrumf), a packet of wavelengths (SpectrumfP), etc. In all cases,
        the PDF is returned per wavelength.

        Parameter ``wavelengths`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:function:: pdf_rgb_spectrum(wavelengths)

        PDF for the sample_rgb_spectrum strategy. It is valid to call this
        function for a single wavelength (Float), a set of wavelengths
        (Spectrumf), a packet of wavelengths (SpectrumfP), etc. In all cases,
        the PDF is returned per wavelength.

        Parameter ``wavelengths`` (:py:obj:`mitsuba.Color3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Color3f`:
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

    Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
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

    Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
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

    Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
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

    Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
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

.. py:function:: mitsuba.reflect(overloaded)


    .. py:function:: reflect(wi)

        Reflection in local coordinates

        Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

    .. py:function:: reflect(wi, m)

        Reflect ``wi`` with respect to a given surface normal

        Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Parameter ``m`` (:py:obj:`mitsuba.Normal3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

.. py:function:: mitsuba.refract(overloaded)


    .. py:function:: refract(wi, cos_theta_t, eta_ti)

        Refraction in local coordinates

        The 'cos_theta_t' and 'eta_ti' parameters are given by the last two
        tuple entries returned by the fresnel and fresnel_polarized functions.

        Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Parameter ``cos_theta_t`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``eta_ti`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

    .. py:function:: refract(wi, m, cos_theta_t, eta_ti)

        Refract ``wi`` with respect to a given surface normal

        Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
            Direction to refract

        Parameter ``m`` (:py:obj:`mitsuba.Normal3f`):
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

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_emitter(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_film(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_integrator(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_medium(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_mesh(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_phasefunction(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_sampler(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_sensor(overloaded)


    .. py:function:: register_sensor(arg0, arg1)

        Parameter ``arg0`` (str):
            *no description available*

        Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.Properties`], object]):
            *no description available*

    .. py:function:: register_sensor(arg0, arg1)

        Parameter ``arg0`` (str):
            *no description available*

        Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.Properties`], object]):
            *no description available*

.. py:function:: mitsuba.register_texture(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.Properties`], object]):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_volume(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (Callable[[:py:obj:`mitsuba.Properties`], object]):
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

    Parameter ``seed`` (``int``)
        This parameter controls the initialization of the random number
        generator during the primal rendering step. It is crucial that you
        specify different seeds (e.g., an increasing sequence) if subsequent
        calls should produce statistically independent images (e.g. to
        de-correlate gradient-based optimization steps).

    Parameter ``seed_grad`` (``int``)
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

    Parameter ``seed`` (int):
        *no description available*

    Parameter ``seed_grad`` (int):
        *no description available*

    Parameter ``spp`` (int):
        *no description available*

    Parameter ``spp_grad`` (int):
        *no description available*

    Returns → mi.TensorXf:
        *no description available*

.. py:function:: mitsuba.sample_rgb_spectrum(overloaded)


    .. py:function:: sample_rgb_spectrum(sample)

        Importance sample a "importance spectrum" that concentrates the
        computation on wavelengths that are relevant for rendering of RGB data

        Based on "An Improved Technique for Full Spectral Rendering" by
        Radziszewski, Boryczko, and Alda

        Returns a tuple with the sampled wavelength and inverse PDF

        Parameter ``sample`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

    .. py:function:: sample_rgb_spectrum(sample)

        Importance sample a "importance spectrum" that concentrates the
        computation on wavelengths that are relevant for rendering of RGB data

        Based on "An Improved Technique for Full Spectral Rendering" by
        Radziszewski, Boryczko, and Alda

        Returns a tuple with the sampled wavelength and inverse PDF

        Parameter ``sample`` (:py:obj:`mitsuba.Color3f`):
            *no description available*

        Returns → Tuple[:py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`]:
            *no description available*

.. py:function:: mitsuba.sample_tea_32(overloaded)


    .. py:function:: sample_tea_32(v0, v1, rounds=4)

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

        Returns → Tuple[int, int]:
            Two uniformly distributed 32-bit integers

    .. py:function:: sample_tea_32(v0, v1, rounds=4)

        Generate fast and reasonably good pseudorandom numbers using the Tiny
        Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

        For details, refer to "GPU Random Numbers via the Tiny Encryption
        Algorithm" by Fahad Zafar, Marc Olano, and Aaron Curtis.

        Parameter ``v0`` (drjit.llvm.ad.UInt):
            First input value to be encrypted (could be the sample index)

        Parameter ``v1`` (drjit.llvm.ad.UInt):
            Second input value to be encrypted (e.g. the requested random
            number dimension)

        Parameter ``rounds`` (int):
            How many rounds should be executed? The default for random number
            generation is 4.

        Returns → Tuple[drjit.llvm.ad.UInt, drjit.llvm.ad.UInt]:
            Two uniformly distributed 32-bit integers

.. py:function:: mitsuba.sample_tea_64(overloaded)


    .. py:function:: sample_tea_64(v0, v1, rounds=4)

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

    .. py:function:: sample_tea_64(v0, v1, rounds=4)

        Generate fast and reasonably good pseudorandom numbers using the Tiny
        Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

        For details, refer to "GPU Random Numbers via the Tiny Encryption
        Algorithm" by Fahad Zafar, Marc Olano, and Aaron Curtis.

        Parameter ``v0`` (drjit.llvm.ad.UInt):
            First input value to be encrypted (could be the sample index)

        Parameter ``v1`` (drjit.llvm.ad.UInt):
            Second input value to be encrypted (e.g. the requested random
            number dimension)

        Parameter ``rounds`` (int):
            How many rounds should be executed? The default for random number
            generation is 4.

        Returns → drjit.llvm.ad.UInt64:
            A uniformly distributed 64-bit integer

.. py:function:: mitsuba.sample_tea_float

    sample_tea_float64(*args, **kwargs)
    Overloaded function.

    1. sample_tea_float64(v0: int, v1: int, rounds: int = 4) -> float

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

    2. sample_tea_float64(v0: drjit.llvm.ad.UInt, v1: drjit.llvm.ad.UInt, rounds: int = 4) -> drjit.llvm.ad.Float64

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

.. py:function:: mitsuba.sample_tea_float32(overloaded)


    .. py:function:: sample_tea_float32(v0, v1, rounds=4)

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

    .. py:function:: sample_tea_float32(v0, v1, rounds=4)

        Generate fast and reasonably good pseudorandom numbers using the Tiny
        Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

        This function uses sample_tea to return single precision floating
        point numbers on the interval ``[0, 1)``

        Parameter ``v0`` (drjit.llvm.ad.UInt):
            First input value to be encrypted (could be the sample index)

        Parameter ``v1`` (drjit.llvm.ad.UInt):
            Second input value to be encrypted (e.g. the requested random
            number dimension)

        Parameter ``rounds`` (int):
            How many rounds should be executed? The default for random number
            generation is 4.

        Returns → drjit.llvm.ad.Float:
            A uniformly distributed floating point number on the interval
            ``[0, 1)``

.. py:function:: mitsuba.sample_tea_float64(overloaded)


    .. py:function:: sample_tea_float64(v0, v1, rounds=4)

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

    .. py:function:: sample_tea_float64(v0, v1, rounds=4)

        Generate fast and reasonably good pseudorandom numbers using the Tiny
        Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

        This function uses sample_tea to return double precision floating
        point numbers on the interval ``[0, 1)``

        Parameter ``v0`` (drjit.llvm.ad.UInt):
            First input value to be encrypted (could be the sample index)

        Parameter ``v1`` (drjit.llvm.ad.UInt):
            Second input value to be encrypted (e.g. the requested random
            number dimension)

        Parameter ``rounds`` (int):
            How many rounds should be executed? The default for random number
            generation is 4.

        Returns → drjit.llvm.ad.Float64:
            A uniformly distributed floating point number on the interval
            ``[0, 1)``

.. py:function:: mitsuba.set_log_level(arg0)

    Parameter ``arg0`` (mitsuba::LogLevel):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.set_variant()

    Set the variant to be used by the `mitsuba` module. Multiple variant
    names can be passed to this function and the first one that is supported
    will be set as current variant.

    Returns → None:
        *no description available*

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

    Returns → Tuple[List[float], List[float]]:
        *no description available*

.. py:function:: mitsuba.spectrum_list_to_srgb(wavelengths, values, bounded=True, d65=False)

    Parameter ``wavelengths`` (List[float]):
        *no description available*

    Parameter ``values`` (List[float]):
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

    Parameter ``wavelengths`` (List[float]):
        Array with the wavelengths to be stored in the file

    Parameter ``values`` (List[float]):
        Array with the values to be stored in the file

    Parameter ``filename`` (:py:obj:`mitsuba.filesystem.path`):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.spline.eval_1d(overloaded)


    .. py:function:: eval_1d(min, max, values, x)

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

        Parameter ``values`` (numpy.ndarray[numpy.float32]):
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

    .. py:function:: eval_1d(nodes, values, x)

        Evaluate a cubic spline interpolant of a *non*-uniformly sampled 1D
        function

        The implementation relies on Catmull-Rom splines, i.e. it uses finite
        differences to approximate the derivatives at the endpoints of each
        spline segment.

        Template parameter ``Extrapolate``:
            Extrapolate values when ``x`` is out of range? (default:
            ``False``)

        Parameter ``nodes`` (numpy.ndarray[numpy.float32]):
            Array containing ``size`` non-uniformly spaced values denoting
            positions the where the function to be interpolated was evaluated.
            They must be provided in *increasing* order.

        Parameter ``values`` (numpy.ndarray[numpy.float32]):
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

    Parameter ``nodes1`` (numpy.ndarray[numpy.float32]):
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

    Parameter ``values`` (numpy.ndarray[numpy.float32]):
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

    Parameter ``nodes2`` (numpy.ndarray[numpy.float32]):
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

    Returns → Tuple[float, float]:
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

    Returns → Tuple[float, float]:
        The definite integral and the interpolated function value at ``t``

.. py:function:: mitsuba.spline.eval_spline_weights(overloaded)


    .. py:function:: eval_spline_weights(min, max, size, x)

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

        Returns → Tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Int, List[drjit.llvm.ad.Float]]:
            A boolean set to ``True`` on success and ``False`` when
            ``Extrapolate=false`` and ``x`` lies outside of [``min``, ``max``]
            and an offset into the function samples associated with weights[0]

    .. py:function:: eval_spline_weights(nodes, x)

        Compute weights to perform a spline-interpolated lookup on a
        *non*-uniformly sampled 1D function.

        The implementation relies on Catmull-Rom splines, i.e. it uses finite
        differences to approximate the derivatives at the endpoints of each
        spline segment. The resulting weights are identical those internally
        used by sample_1d().

        Template parameter ``Extrapolate``:
            Extrapolate values when ``x`` is out of range? (default:
            ``False``)

        Parameter ``nodes`` (numpy.ndarray[numpy.float32]):
            Array containing ``size`` non-uniformly spaced values denoting
            positions the where the function to be interpolated was evaluated.
            They must be provided in *increasing* order.

        Parameter ``size``:
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

        Returns → Tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Int, List[drjit.llvm.ad.Float]]:
            A boolean set to ``True`` on success and ``False`` when
            ``Extrapolate=false`` and ``x`` lies outside of [``min``, ``max``]
            and an offset into the function samples associated with weights[0]

.. py:function:: mitsuba.spline.integrate_1d(overloaded)


    .. py:function:: integrate_1d(min, max, values)

        Computes a prefix sum of integrals over segments of a *uniformly*
        sampled 1D Catmull-Rom spline interpolant

        This is useful for sampling spline segments as part of an importance
        sampling scheme (in conjunction with sample_1d)

        Parameter ``min`` (float):
            Position of the first node

        Parameter ``max`` (float):
            Position of the last node

        Parameter ``values`` (numpy.ndarray[numpy.float32]):
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

        Returns → drjit.scalar.ArrayXf:
            *no description available*

    .. py:function:: integrate_1d(nodes, values)

        Computes a prefix sum of integrals over segments of a *non*-uniformly
        sampled 1D Catmull-Rom spline interpolant

        This is useful for sampling spline segments as part of an importance
        sampling scheme (in conjunction with sample_1d)

        Parameter ``nodes`` (numpy.ndarray[numpy.float32]):
            Array containing ``size`` non-uniformly spaced values denoting
            positions the where the function to be interpolated was evaluated.
            They must be provided in *increasing* order.

        Parameter ``values`` (numpy.ndarray[numpy.float32]):
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

.. py:function:: mitsuba.spline.invert_1d(overloaded)


    .. py:function:: invert_1d(min, max_, values, y, eps=9.999999974752427e-07)

        Invert a cubic spline interpolant of a *uniformly* sampled 1D
        function. The spline interpolant must be *monotonically increasing*.

        Parameter ``min`` (float):
            Position of the first node

        Parameter ``max``:
            Position of the last node

        Parameter ``values`` (numpy.ndarray[numpy.float32]):
            Array containing ``size`` regularly spaced evaluations in the
            range [``min``, ``max``] of the approximated function.

        Parameter ``size``:
            Denotes the size of the ``values`` array

        Parameter ``y`` (drjit.llvm.ad.Float):
            Input parameter for the inversion

        Parameter ``eps`` (float):
            Error tolerance (default: 1e-6f)

        Returns → drjit.llvm.ad.Float:
            The spline parameter ``t`` such that ``eval_1d(..., t)=y``

        Parameter ``max_`` (float):
            *no description available*

    .. py:function:: invert_1d(nodes, values, y, eps=9.999999974752427e-07)

        Invert a cubic spline interpolant of a *non*-uniformly sampled 1D
        function. The spline interpolant must be *monotonically increasing*.

        Parameter ``nodes`` (numpy.ndarray[numpy.float32]):
            Array containing ``size`` non-uniformly spaced values denoting
            positions the where the function to be interpolated was evaluated.
            They must be provided in *increasing* order.

        Parameter ``values`` (numpy.ndarray[numpy.float32]):
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

.. py:function:: mitsuba.spline.sample_1d(overloaded)


    .. py:function:: sample_1d(min, max, values, cdf, sample, eps=9.999999974752427e-07)

        Importance sample a segment of a *uniformly* sampled 1D Catmull-Rom
        spline interpolant

        Parameter ``min`` (float):
            Position of the first node

        Parameter ``max`` (float):
            Position of the last node

        Parameter ``values`` (numpy.ndarray[numpy.float32]):
            Array containing ``size`` regularly spaced evaluations in the
            range [``min``, ``max``] of the approximated function.

        Parameter ``cdf`` (numpy.ndarray[numpy.float32]):
            Array containing a cumulative distribution function computed by
            integrate_1d().

        Parameter ``size``:
            Denotes the size of the ``values`` array

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A uniformly distributed random sample in the interval ``[0,1]``

        Parameter ``eps`` (float):
            Error tolerance (default: 1e-6f)

        Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            1. The sampled position 2. The value of the spline evaluated at
            the sampled position 3. The probability density at the sampled
            position (which only differs from item 2. when the function does
            not integrate to one)

    .. py:function:: sample_1d(nodes, values, cdf, sample, eps=9.999999974752427e-07)

        Importance sample a segment of a *non*-uniformly sampled 1D Catmull-
        Rom spline interpolant

        Parameter ``nodes`` (numpy.ndarray[numpy.float32]):
            Array containing ``size`` non-uniformly spaced values denoting
            positions the where the function to be interpolated was evaluated.
            They must be provided in *increasing* order.

        Parameter ``values`` (numpy.ndarray[numpy.float32]):
            Array containing function evaluations matched to the entries of
            ``nodes``.

        Parameter ``cdf`` (numpy.ndarray[numpy.float32]):
            Array containing a cumulative distribution function computed by
            integrate_1d().

        Parameter ``size``:
            Denotes the size of the ``values`` array

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A uniformly distributed random sample in the interval ``[0,1]``

        Parameter ``eps`` (float):
            Error tolerance (default: 1e-6f)

        Returns → Tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            1. The sampled position 2. The value of the spline evaluated at
            the sampled position 3. The probability density at the sampled
            position (which only differs from item 2. when the function does
            not integrate to one)

.. py:function:: mitsuba.srgb_model_eval(arg0, arg1)

    Parameter ``arg0`` (drjit.llvm.ad.Array3f):
        *no description available*

    Parameter ``arg1`` (:py:obj:`mitsuba.Color0f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Color3f`:
        *no description available*

.. py:function:: mitsuba.srgb_model_fetch(arg0)

    Look up the model coefficients for a sRGB color value

    Parameter ``c``:
        An sRGB color value where all components are in [0, 1].

    Parameter ``arg0`` (:py:obj:`mitsuba.ScalarColor3f`):
        *no description available*

    Returns → drjit.scalar.Array3f:
        Coefficients for use with srgb_model_eval

.. py:function:: mitsuba.srgb_model_mean(arg0)

    Parameter ``arg0`` (drjit.llvm.ad.Array3f):
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

.. py:function:: mitsuba.unpolarized_spectrum(arg0)

    Parameter ``arg0`` (:py:obj:`mitsuba.Color3f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Color3f`:
        *no description available*

.. py:function:: mitsuba.util.convert_to_bitmap()

    Convert the RGB image in `data` to a `Bitmap`. `uint8_srgb` defines whether
    the resulting bitmap should be translated to a uint8 sRGB bitmap.

.. py:function:: mitsuba.util.core_count()

    Determine the number of available CPU cores (including virtual cores)

    Returns → int:
        *no description available*

.. py:function:: mitsuba.util.mem_string(size, precise=False)

    Turn a memory size into a human-readable string

    Parameter ``size`` (int):
        *no description available*

    Parameter ``precise`` (bool):
        *no description available*

    Returns → str:
        *no description available*

.. py:function:: mitsuba.util.time_string(time, precise=False)

    Convert a time difference (in seconds) to a string representation

    Parameter ``time`` (float):
        Time difference in (fractional) sections

    Parameter ``precise`` (bool):
        When set to true, a higher-precision string representation is
        generated.

    Returns → str:
        *no description available*

.. py:function:: mitsuba.util.trap_debugger()

    Generate a trap instruction if running in a debugger; otherwise,
    return.

    Returns → None:
        *no description available*

.. py:function:: mitsuba.util.write_bitmap()

    Write the RGB image in `data` to a PNG/EXR/.. file.

.. py:function:: mitsuba.variant()

    Return currently enabled variant

    Returns → str:
        *no description available*

.. py:function:: mitsuba.variant_context()

    Temporarily override the active variant. Arguments are interpreted as
    they are in :func:`mitsuba.set_variant`.

    Returns → None:
        *no description available*

.. py:function:: mitsuba.variants()

    Return a list of all variants that have been compiled

    Returns → ~typing.List[str]:
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

    Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

    Returns → Tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
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

    Parameter ``kappa`` (float):
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

    Parameter ``kappa`` (float):
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

    Returns → List[Tuple[str, :py:obj:`mitsuba.Properties`]]:
        *no description available*

.. py:function:: mitsuba.xyz_to_srgb(rgb, active=True)

    Convert XYZ tristimulus values to ITU-R Rec. BT.709 linear RGB

    Parameter ``rgb`` (:py:obj:`mitsuba.Color3f`):
        *no description available*

    Parameter ``active`` (drjit.llvm.ad.Bool):
        Mask to specify active lanes.

    Returns → :py:obj:`mitsuba.Color3f`:
        *no description available*

