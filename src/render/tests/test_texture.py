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


def test03_python_field_api(variants_vec_backends_once_rgb):
    surface_field = mi.load_dict({
        "type": "uniform",
        "value": 1.0,
    })
    interaction_field = mi.load_dict({
        "type": "constvolume",
        "value": 1.0,
    })

    assert isinstance(surface_field, mi.Field)
    assert isinstance(interaction_field, mi.Field)
    assert mi.PluginManager.instance().plugin_type("uniform") == mi.ObjectType.Field
    assert mi.PluginManager.instance().plugin_type("constvolume") == mi.ObjectType.Field
    assert surface_field.domain() == mi.FieldDomain.Surface
    assert interaction_field.domain() == mi.FieldDomain.Interaction

    illuminant = mi.Field.D65()
    assert isinstance(illuminant, mi.Field)
