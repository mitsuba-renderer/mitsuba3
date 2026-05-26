import pytest
import drjit as dr
import mitsuba as mi


CONFIG = mi.parser.ParserConfig("scalar_rgb")


def surface_interaction(width=1):
    si = dr.zeros(mi.SurfaceInteraction3f, width)
    si.p = mi.Point3f(0.25, 0.5, 0.75)
    si.uv = mi.Point2f(0.25, 0.75)
    si.n = mi.Normal3f(0, 0, 1)
    si.wi = mi.Vector3f(0, 0, 1)
    si.sh_frame = mi.Frame3f(si.n)
    return si


def bitmap_field(value):
    return {
        "type": "bitmap",
        "data": mi.TensorXf(list(value) * 4, shape=(2, 2, len(value))),
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }


def make_python_args_field():
    class PythonArgsField(mi.Field):
        def __init__(self):
            super().__init__(mi.Properties("python_args_field"))

        def out_type(self):
            return mi.FieldValueType.Color3

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            return 3

        def args_dim(self):
            return 4

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

        def eval_color3(self, si, args=None, active=True):
            assert args is not None
            return mi.Color3f(args[0] + si.uv.x, args[1] + si.uv.y, args[2] + args[3])

        def eval(self, si, args=None, active=True):
            value = self.eval_color3(si, args=args, active=active)
            return mi.ArrayXf([value.x, value.y, value.z])

        def eval_1(self, si, args=None, active=True):
            return self.eval_color3(si, args=args, active=active).x

        def eval_n(self, si, count, args=None, active=True):
            if count != 3:
                raise RuntimeError("count must be 3")
            return mi.ArrayXf(self.eval_color3(si, args=args, active=active))

    return PythonArgsField()


def test01_xml_field_tags_round_trip_nested_refs_and_ordering(variant_scalar_rgb):
    xml = """<scene version="3.0.0">
        <shape type="rectangle"/>
        <field type="debugfield" id="f0">
            <integer name="out_dim" value="3"/>
        </field>
        <bsdf type="neuralbsdf" id="mat_nested">
            <field name="reflectance" type="debugfield" id="nested_f">
                <integer name="out_dim" value="3"/>
            </field>
        </bsdf>
        <bsdf type="neuralbsdf" id="mat_ref">
            <ref name="reflectance" id="f0"/>
        </bsdf>
    </scene>"""

    state = mi.parser.parse_string(CONFIG, xml)
    field_nodes = [n for n in state.nodes if n.type == mi.ObjectType.Field]
    assert [n.props.plugin_name() for n in field_nodes] == ["debugfield", "debugfield"]
    assert [n.props.id() for n in field_nodes] == ["f0", "nested_f"]

    output = mi.parser.write_string(state)
    assert '<field' in output
    assert 'type="debugfield"' in output
    assert 'id="f0"' in output
    assert 'name="reflectance"' in output
    assert 'id="nested_f"' in output
    assert '<ref name="reflectance" id="f0"/>' in output

    state_rt = mi.parser.parse_string(CONFIG, output)
    field_nodes_rt = [n for n in state_rt.nodes if n.type == mi.ObjectType.Field]
    assert len(field_nodes_rt) == 2
    assert all(n.props.plugin_name() == "debugfield" for n in field_nodes_rt)


def test02_python_field_metadata_and_eval_dispatch_through_virtual_api(field_ad_rgb_variant):
    field = make_python_args_field()
    si = surface_interaction()

    assert field.out_type() == mi.FieldValueType.Color3
    assert field.domain() == mi.FieldDomain.Surface
    assert field.out_dim() == 3
    assert field.args_dim() == 4
    assert field.supports_scalar()
    assert field.supports_jit()
    assert field.supports_surface_queries()
    assert not field.supports_interaction_queries()

    assert dr.allclose(field.eval_color3(si, args=[0.1, 0.2, 0.3, 0.4]),
                       [0.35, 0.95, 0.7])
    assert dr.allclose(field.eval(si, args=[0.1, 0.2, 0.3, 0.4]),
                       [0.35, 0.95, 0.7])
    assert dr.allclose(field.eval_1(si, args=mi.ArrayXf([0.1, 0.2, 0.3, 0.4])),
                       0.35)
    assert dr.allclose(field.eval_n(si, 3, args=[0.1, 0.2, 0.3, 0.4]),
                       [0.35, 0.95, 0.7])

    with pytest.raises(RuntimeError, match="args_dim|expected 4|got 0"):
        field.eval(si)
    with pytest.raises(RuntimeError, match="args_dim|expected 4|got 0"):
        field.eval_color3(si)
    with pytest.raises(RuntimeError, match="args_dim|4"):
        field.eval_color3(si, args=[1.0, 2.0, 3.0])
    with pytest.raises(RuntimeError, match="args_dim|4"):
        field.eval_n(si, 3, args=[1.0, 2.0, 3.0])


def test03_fieldptr_vectorized_fixed_and_generic_calls(field_ad_rgb_variant):
    color_a = mi.load_dict(bitmap_field([0.1, 0.2, 0.3])).field()
    color_b = mi.load_dict(bitmap_field([0.7, 0.8, 0.9])).field()

    ptr = dr.zeros(mi.FieldPtr, 4)
    dr.scatter(ptr, color_a, mi.UInt32(0, 2))
    dr.scatter(ptr, color_b, mi.UInt32(1, 3))

    si = surface_interaction(width=4)
    result = ptr.eval_color3(si, True)
    expected = mi.Color3f(
        mi.Float(0.1, 0.7, 0.1, 0.7),
        mi.Float(0.2, 0.8, 0.2, 0.8),
        mi.Float(0.3, 0.9, 0.3, 0.9),
    )
    assert dr.allclose(result, expected)

    scalar_a = mi.load_dict(bitmap_field([1.0])).field()
    scalar_b = mi.load_dict(bitmap_field([2.0])).field()
    scalar_ptr = dr.zeros(mi.FieldPtr, 4)
    dr.scatter(scalar_ptr, scalar_a, mi.UInt32(0, 2))
    dr.scatter(scalar_ptr, scalar_b, mi.UInt32(1, 3))

    assert dr.allclose(scalar_ptr.eval_1(si, True), mi.Float(1, 2, 1, 2))
    generic = scalar_ptr.eval(si, True)
    assert dr.allclose(generic[0], mi.Float(1, 2, 1, 2))
