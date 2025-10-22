import pytest
import mitsuba as mi


def test01_variants_callbacks(variants_all_backends_once):
    available = mi.variants()
    if len(available) <= 1:
        pytest.mark.skip("Test requires more than 1 enabled variant")

    history = []
    change_count = 0
    def track_changes(old, new):
        history.append((old, new))
    def count_changes(old, new):
        nonlocal change_count
        change_count += 1

    mi.detail.add_variant_callback(track_changes)
    mi.detail.add_variant_callback(count_changes)
    # Adding the same callback multiple times does nothing.
    # It won't be called multiple times.
    mi.detail.add_variant_callback(track_changes)
    mi.detail.add_variant_callback(track_changes)

    try:
        previous = mi.variant()
        base_i = available.index(previous)

        expected = []
        for i in range(1, len(available) + 1):
            next_i = (base_i + i) % len(available)
            next_variant = available[next_i]

            assert next_variant != mi.variant()
            mi.set_variant(next_variant)
            expected.append((previous, next_variant))
            previous = next_variant

        assert len(expected) > 1
        assert len(history) == len(expected)
        assert change_count == len(expected)
        for e, h in zip(expected, history):
            assert h == e

    finally:
        # The callback shouldn't stay on even if the test fails.
        mi.detail.remove_variant_callback(track_changes)

    # Callback shouldn't be called anymore
    len_e = len(expected)
    next_variant = available[(available.index(mi.variant()) + 1) % len(available)]
    with mi.util.scoped_set_variant(next_variant):
        pass
    assert len(expected) == len_e
