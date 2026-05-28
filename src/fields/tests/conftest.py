import pytest
import drjit as dr
import mitsuba as mi


_FIELD_AD_RGB_VARIANTS = [
    ("cuda_ad_rgb", dr.JitBackend.CUDA),
    ("llvm_ad_rgb", dr.JitBackend.LLVM),
]


def _variant_available(variant, backend):
    return variant in mi.variants() and dr.has_backend(backend)


@pytest.fixture(params=[
    pytest.param(entry, id=entry[0]) for entry in _FIELD_AD_RGB_VARIANTS
])
def field_ad_rgb_variant(request):
    variant, backend = request.param
    if not _variant_available(variant, backend):
        pytest.skip(f"{variant} is not available")

    mi.set_variant(variant)
    try:
        yield variant
    finally:
        dr.sync_thread()
        dr.flush_kernel_cache()
        dr.kernel_history_clear()
        dr.flush_malloc_cache()
        dr.detail.malloc_clear_statistics()
        dr.detail.clear_registry()
        dr.set_flag(dr.JitFlag.Default, True)
