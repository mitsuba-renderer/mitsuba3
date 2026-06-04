import drjit as dr
import mitsuba as mi


def register_field_once(name, constructor):
    try:
        mi.register_field(name, constructor)
    except RuntimeError as exc:
        if "already" not in str(exc).lower():
            raise


def test01_fieldptr_surface_vcalls(variants_vec_backends_once_rgb):
    field1 = mi.load_dict({
        "type": "uniform",
        "value": 5.0,
    })
    field2 = mi.load_dict({
        "type": "uniform",
        "value": 28,
    })
    field3 = mi.load_dict({
        "type": "uniform",
        "value": 133,
    })

    fields = dr.zeros(mi.FieldPtr, 6)
    dr.scatter(fields, field1, mi.UInt32(0, 2))
    dr.scatter(fields, field2, mi.UInt32(1, 3))
    dr.scatter(fields, field3, mi.UInt32(4, 5))

    si = dr.zeros(mi.SurfaceInteraction3f)
    result = fields.eval_1(si, True)

    assert dr.allclose(result, mi.Float(5.0, 28.0, 5.0, 28.0, 133.0, 133.0))


def test02_python_field_trampoline(variants_vec_backends_once_rgb):
    class DummyField(mi.Field):
        def __init__(self, props):
            mi.Field.__init__(self, props)
            self.value = props.get("value")

        def out_type(self):
            return mi.FieldValueType.Float

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            return 1

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

        def eval_1(self, si, args=None, active=True):
            return dr.select(active, self.value, 0.0)

    register_field_once("dummy_surface_field", DummyField)
    field = mi.load_dict({
        "type": "dummy_surface_field",
        "value": 96.0,
    })

    si = dr.zeros(mi.SurfaceInteraction3f)
    assert dr.allclose(field.eval_1(si, active=True), 96.0)

    ptr = dr.zeros(mi.FieldPtr, 4)
    dr.scatter(ptr, field, mi.UInt32(0, 1, 2, 3))
    assert dr.allclose(ptr.eval_1(si, True), 96.0)


def test03_legacy_texture_volume_python_api_removed(variants_vec_backends_once_rgb):
    assert not hasattr(mi, "Texture")
    assert not hasattr(mi, "TexturePtr")
    assert not hasattr(mi, "Volume")
    assert not hasattr(mi, "VolumePtr")
    assert not hasattr(mi, "register_texture")
    assert not hasattr(mi, "register_volume")

    field = mi.Field.D65()
    assert isinstance(field, mi.Field)
