from __future__ import annotations # Delayed parsing of type annotations


_REGISTERED_VARIANTS: set[str] = set()


def _register_variant_fields(mi=None) -> None:
    if mi is None:
        import mitsuba as mi

    variant = mi.variant()
    if variant is None or variant in _REGISTERED_VARIANTS:
        return

    from .hashgrid import _make_hashgrid_field
    from .permuto import _make_permuto_field
    from .neural import _make_neural_field

    # Field subclasses are variant-specific because they derive from mi.Field.
    mi.register_field("hashgridfield", _make_hashgrid_field(mi))
    mi.register_field("permutofield", _make_permuto_field(mi))
    mi.register_field("neuralfield", _make_neural_field(mi))
    _REGISTERED_VARIANTS.add(variant)
