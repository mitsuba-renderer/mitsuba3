import pytest
import drjit as dr
import mitsuba as mi


def _variant_available(variant, backend):
    return variant in mi.variants() and dr.has_backend(backend)


@pytest.fixture()
def field_ad_rgb_variant():
    for variant, backend in [
        ("cuda_ad_rgb", dr.JitBackend.CUDA),
        ("llvm_ad_rgb", dr.JitBackend.LLVM),
    ]:
        if _variant_available(variant, backend):
            mi.set_variant(variant)
            yield variant
            dr.sync_thread()
            dr.flush_kernel_cache()
            dr.kernel_history_clear()
            dr.flush_malloc_cache()
            dr.detail.malloc_clear_statistics()
            dr.detail.clear_registry()
            dr.set_flag(dr.JitFlag.Default, True)
            return

    pytest.skip("field AD tests require cuda_ad_rgb or llvm_ad_rgb")
