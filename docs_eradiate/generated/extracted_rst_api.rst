.. py:data:: mitsuba.ERADIATE_MI_VERSION
    :type: str
    :value: 0.6.0

.. py:data:: mitsuba.ERADIATE_MI_VERSION_MAJOR
    :type: int
    :value: 0

.. py:data:: mitsuba.ERADIATE_MI_VERSION_MINOR
    :type: int
    :value: 6

.. py:data:: mitsuba.ERADIATE_MI_VERSION_PATCH
    :type: int
    :value: 0

.. py:class:: mitsuba.ExtremumSegment

    Stores the extremum (minorant/majorant) data for a ray segment.

    Used as the output type of ExtremumStructure traversal. Tracks the
    segment's entry/exit distances and the local extinction coefficient
    bounds within that interval.

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        
        2. ``__init__(self, other: :py:obj:`mitsuba.ExtremumSegment`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, mint: float, maxt: float, minorant: float, majorant: float) -> None``
        
        Construct from entry/exit distances and a combined extremum vector.
        
        Parameter ``mint``:
            Segment entry distance
        
        Parameter ``maxt``:
            Segment exit distance
        
        Parameter ``value``:
            Extremum vector [minorant, majorant]
        
        4. ``__init__(self, mint: float, maxt: float, value: :py:obj:`mitsuba.Vector2f`) -> None``
        
        Construct from entry/exit distances and a combined extremum vector.
        
        Parameter ``mint``:
            Segment entry distance
        
        Parameter ``maxt``:
            Segment exit distance
        
        Parameter ``value``:
            Extremum vector [minorant, majorant]

        
    .. py:method:: mitsuba.ExtremumSegment.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.ExtremumSegment`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ExtremumSegment.majorant()

        Majorant value over the segment. Accessor to the second element of
        ``value``.

        Returns → float:
            *no description available*

    .. py:property:: mitsuba.ExtremumSegment.maxt

        Segment exit distance along ray

    .. py:method:: mitsuba.ExtremumSegment.minorant()

        Minorant value over the segment. Accessor to the first element of
        ``value``.

        Returns → float:
            *no description available*

    .. py:property:: mitsuba.ExtremumSegment.mint

        Segment entry distance along ray

    .. py:method:: mitsuba.ExtremumSegment.reset()

        Mark the extremum segment as invalid.

        This operation sets segment's minimum and maximum distances to
        :math:`\infty` and :math:`-\infty`, respectively.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ExtremumSegment.valid()

        Check whether this is a valid segment

        A segment is considered valid when

        .. code-block:: c

            segment.mint < segment.maxt


        Returns → bool:
            *no description available*

    .. py:property:: mitsuba.ExtremumSegment.value

        Extremum data stored as [minorant, majorant]

    .. py:method:: mitsuba.ExtremumSegment.zero_(self, size=1)

        Overloaded function.

        1. ``zero_(self, size: int = 1) -> None``


        2. ``zero_(self, arg: int, /) -> None``

        This callback method is invoked by dr::zeros<>, and takes care of
        fields that deviate from the standard zero-initialization convention.
        In ExtremumSegment, the ``mint`` and ``maxt`` fields are set to + and
        - infinity respectively to to mark invalid intersection records.

        Parameter ``size`` (int):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.ExtremumStructure

    Base class: :py:obj:`mitsuba.Object`

    Abstract base class for extremum structures

    ExtremumStructure provides an interface for spatial data structures
    that store local extrema (majorant/minorant) of volumetric extinction
    coefficients. This enables efficient delta tracking with locally-
    adaptive majorants.

    To minimize virtual function overhead, the ``traverse_extremum()``
    method encapsulates the entire traversal loop internally, requiring
    only a single virtual call per distance sample.

    .. py:method:: __init__(self, props)

        Parameter ``props`` (:py:obj:`mitsuba.Properties`):
            *no description available*


    .. py:method:: mitsuba.ExtremumStructure.bbox()

        Return the bounding box of the extremum structure

        Returns → :py:obj:`mitsuba.BoundingBox3f`:
            *no description available*

    .. py:method:: mitsuba.ExtremumStructure.eval_1(self, it, active=True)

        Evaluate the minorant and majorant at a medium interaction point.

        This method performs point evaluation at interaction point specified
        in local space.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            Interaction interaction point in local space

        Parameter ``active`` (bool):
            Mask for active lanes

        Returns → tuple[float, float]:
            The minorant and majorant values at the medium interaction point.
            Clamped values outside bounds.

.. py:class:: mitsuba.Medium

    Base class: :py:obj:`mitsuba.Object`

    .. py:method:: mitsuba.Medium.extremum_structure()

        Returns the extremum structure for local extremum acceleration.

        Returns → :py:obj:`mitsuba.ExtremumStructure`:
            *no description available*

    .. py:method:: mitsuba.Medium.get_majorant(self, mi, active=True)

        Returns the medium's majorant used for delta tracking

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            *no description available*

    .. py:method:: mitsuba.Medium.get_scattering_coefficients(self, mi, active=True)

        Returns the medium coefficients Sigma_s, Sigma_n and Sigma_t evaluated
        at a given MediumInteraction mi

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`]:
            *no description available*

    .. py:method:: mitsuba.Medium.has_spectral_extinction()

        Returns whether this medium has a spectrally varying extinction

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Medium.intersect_aabb(self, ray)

        Intersects a ray with the medium's bounding box

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Returns → tuple[bool, float, float]:
            *no description available*

    .. py:method:: mitsuba.Medium.is_homogeneous()

        Returns whether this medium is homogeneous

        Returns → bool:
            *no description available*

    .. py:property:: mitsuba.Medium.m_has_spectral_extinction

        (self) -> bool

    .. py:property:: mitsuba.Medium.m_is_homogeneous

        (self) -> bool

    .. py:property:: mitsuba.Medium.m_sample_emitters

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

        Parameter ``sample`` (float):
            A uniformly distributed random sample

        Parameter ``channel`` (int):
            The channel according to which we will sample the free-flight
            distance. This argument is only used when rendering in RGB modes.

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.MediumInteraction3f`:
            This method returns a MediumInteraction. The MediumInteraction
            will always be valid, except if the ray missed the Medium's
            bounding box.

    .. py:method:: mitsuba.Medium.sample_interaction_analytical(self, ray, it, sample, channel, active)

        Sample a free-flight distance in the medium analytically.

        This function samples a (tentative) free-flight distance according to
        an exponential transmittance. It is then up to the integrator to then
        decide whether the MediumInteraction corresponds to a real or null
        scattering event.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            Ray, along which a distance should be sampled

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            The boundary interaction that the sampled distance cannot exceed.

        Parameter ``sample`` (float):
            A uniformly distributed random sample

        Parameter ``channel`` (int):
            The channel according to which we will sample the free-flight
            distance. This argument is only used when rendering in RGB modes.

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.MediumInteraction3f`, :py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`]:
            This method returns a tuple (MediumInteraction, Transmittance,
            PDF). The MediumInteraction if an interaction was sampled within
            the medium boudning box and before the bouding iteraction it. The
            transmittance and PDF are both computed for all channels even if
            the sampling operation is performed on one channel.

    .. py:method:: mitsuba.Medium.transmittance_eval_analytical(self, ray, it, active)

        Compute the analytical transmittance along a ray to an interaction.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            Ray, along which to compute the transmittance, use mint

        Parameter ``si``:
            Interaction that marks the end of the segment along which to
            compute the transmittance.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The transmittance along a ray

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

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`]:
            This method returns a pair of (Transmittance, PDF).

    .. py:method:: mitsuba.Medium.use_emitter_sampling()

        Returns whether this specific medium instance uses emitter sampling

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.PhaseFunction

    Base class: :py:obj:`mitsuba.Object`

    .. py:method:: mitsuba.PhaseFunction.accumulate_envelope(self, nodes, values)

        Evaluate the phase function at the given cos_theta nodes and
        accumulate the result into ``values`` by taking the elementwise
        maximum.

        For each node :math:`\mu_i` in ``nodes`` this method evaluates the
        phase function value :math:`p(\mu_i)` and updates
        :math:`\texttt{values}[i] \leftarrow \max(\texttt{values}[i],\,
        p(\mu_i))`.

        Delegating the comparison to the callee rather than the caller enables
        natural recursion through composite phase functions (e.g.
        BlendPhaseFunction, MultiPhaseFunction): a composite implementation
        simply calls ``accumulate_envelope`` on each child with the same
        buffer, and each child accumulates its contribution independently. The
        resulting buffer holds the pointwise supremum over the entire phase-
        function tree without the caller needing to know its structure.

        \note cos_theta follows the physics convention (see
        get_envelope_nodes).

        Parameter ``nodes`` (drjit.scalar.ArrayXf):
            cos_theta values at which to evaluate the phase function, as
            returned by get_envelope_nodes.

        Parameter ``values`` (drjit.scalar.ArrayXf):
            In/out buffer. Must have the same length as ``nodes`` and be zero-
            initialised before the first comparison. On return, each entry
            holds the maximum of its previous value and the phase function
            evaluated at the corresponding node.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.PhaseFunction.component_count(self, active=True)

        Number of components this phase function is comprised of.

        Parameter ``active`` (bool):
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

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, float]:
            The value and the sampling PDF of the phase function in direction
            wo

    .. py:method:: mitsuba.PhaseFunction.flags(self, index, active=True)

        Overloaded function.

        1. ``flags(self, index: int, active: bool = True) -> int``

        Flags for a specific component of this phase function.

        2. ``flags(self, active: bool = True) -> int``

        Flags for this phase function.

        Parameter ``index`` (int):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.PhaseFunction.get_envelope_nodes()

        Populate a set of cos_theta nodes suitable for representing this phase
        function.

        The nodes are used to build the piecewise-linear envelope required by
        the DDIS importance sampling scheme. Subclasses may override this
        method to supply irregularly spaced nodes that better resolve sharp
        features (e.g. a strong forward-scattering peak). The default
        implementation places ``m_node_count`` nodes uniformly in [-1, 1].

        \note cos_theta follows the physics convention: a value of +1
        corresponds to aligned (forward-scattering) incoming and outgoing
        directions, and -1 corresponds to exact backscatter.

        Returns → drjit.scalar.ArrayXf:
            A sorted buffer of cos_theta values at which the phase function
            should be evaluated.

    .. py:property:: mitsuba.PhaseFunction.m_flags

        Type of phase function (e.g. anisotropic)

    .. py:method:: mitsuba.PhaseFunction.max_projected_area()

        Return the maximum projected area of the microflake distribution

        Returns → float:
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

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
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

        Parameter ``sample1`` (float):
            A uniformly distributed sample on :math:`[0,1]`. It is used to
            select the phase function component in multi-component models.

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on :math:`[0,1]^2`. It is used to
            generate the sampled direction.

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Vector3f`, :py:obj:`mitsuba.Color3f`, float]:
            A sampled direction wo and its corresponding weight and PDF

