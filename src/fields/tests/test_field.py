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


def scalar_bitmap_field(values):
    return {
        "type": "bitmap",
        "data": mi.TensorXf(values, shape=(2, 2, 1)),
        "raw": True,
        "filter_type": "bilinear",
        "wrap_mode": "clamp",
    }


def register_nested_test_fields():
    class TestCoordField(mi.Field):
        def __init__(self, props):
            mi.Field.__init__(self, props)

        def out_type(self):
            return mi.FieldValueType.Float

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            return 1

        def args_dim(self):
            return 0

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

        def is_spatially_varying(self):
            return True

        def eval_1(self, si, args=None, active=True):
            return dr.select(active, 2.0 * si.uv.x + 3.0 * si.uv.y, 0.0)

    class TestAffineField(mi.Field):
        def __init__(self, props):
            mi.Field.__init__(self, props)
            self.child = props["child"]
            self.scale = props.get("scale", 1.0)
            self.bias = props.get("bias", 0.0)

        def out_type(self):
            return mi.FieldValueType.Float

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            return 1

        def args_dim(self):
            return 0

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

        def is_spatially_varying(self):
            return self.child.is_spatially_varying()

        def traverse(self, cb):
            cb.put("child", self.child, mi.ParamFlags.Differentiable)

        def eval_1(self, si, args=None, active=True):
            value = self.scale * self.child.eval_1(si, active=active) + self.bias
            return dr.select(active, value, 0.0)

    for name, constructor in [
        ("test_coordfield", TestCoordField),
        ("test_affinefield", TestAffineField),
    ]:
        try:
            mi.register_field(name, constructor)
        except RuntimeError as exc:
            if "already" not in str(exc).lower():
                raise


def affine_field(child, scale, bias):
    return {
        "type": "test_affinefield",
        "child": child,
        "scale": scale,
        "bias": bias,
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
    assert dr.allclose(field.eval_color3(si, [0.1, 0.2, 0.3, 0.4]),
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


def test03_python_field_subclass_hook_preserves_user_hooks(field_ad_rgb_variant):
    class HookedFieldBase(mi.Field):
        initialized_subclasses = []

        def __init__(self):
            mi.Field.__init__(self, mi.Properties("hooked_field"))

        def __init_subclass__(cls, label=None, **kwargs):
            super().__init_subclass__(**kwargs)
            cls.label = label
            HookedFieldBase.initialized_subclasses.append(cls.__name__)

        def out_type(self):
            return mi.FieldValueType.Color3

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            return 3

        def args_dim(self):
            return 1

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

    class HookedField(HookedFieldBase, label="child"):
        def eval_color3(self, si, args=None, active=True):
            return mi.Color3f(args[0] + si.uv.x, si.uv.y, 1.0)

    field = HookedField()
    si = surface_interaction()

    assert HookedField.label == "child"
    assert "HookedField" in HookedFieldBase.initialized_subclasses
    assert dr.allclose(field.eval_color3(si, args=[0.5]), [0.75, 0.75, 1.0])
    with pytest.raises(RuntimeError, match="args_dim|expected 1|got 0"):
        field.eval_color3(si)


def test04_fieldptr_vectorized_fixed_and_generic_calls(field_ad_rgb_variant):
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

    features = mi.load_dict({
        "type": "sinusoidalfield",
        "input_dim": 2,
        "out_dim": 6,
    })
    feature_ptr = dr.zeros(mi.FieldPtr, 4)
    dr.scatter(feature_ptr, features, mi.UInt32(0, 1, 2, 3))
    assert dr.allclose(feature_ptr.eval_n(si, 6, True), features.eval(si))
    with pytest.raises(RuntimeError, match="FieldPtr::eval"):
        feature_ptr.eval(si, True)


def test05_python_surface_array3_eval_n(variant_scalar_rgb):
    class Array3Field(mi.Field):
        def __init__(self, props):
            mi.Field.__init__(self, props)

        def out_type(self):
            return mi.FieldValueType.Array3

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            return 3

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

        def eval_n(self, si, count, args=None, active=True):
            assert count == 3
            return [si.uv.x, si.uv.y, si.uv.x + si.uv.y]

    texture = Array3Field(mi.Properties("array3_field"))
    si = surface_interaction()

    assert dr.allclose(texture.eval_n(si, 3), [0.25, 0.75, 1.0])
    assert dr.allclose(mi.Field.eval_n(texture, si, 3), [0.25, 0.75, 1.0])


def test06_python_field_legacy_overrides_work_through_base_dispatch(variant_scalar_rgb):
    class LegacyScalarField(mi.Field):
        def __init__(self):
            mi.Field.__init__(self, mi.Properties("legacy_scalar_field"))

        def out_type(self):
            return mi.FieldValueType.Float

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            return 1

        def args_dim(self):
            return 0

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

        def eval(self, si, active=True):
            return mi.ArrayXf([self.eval_1(si, active)])

        def eval_1(self, si, active=True):
            return dr.select(active, 2.0 * si.uv.x, 0.0)

        def eval_n(self, si, count, active=True):
            if count != 1:
                raise RuntimeError("count must be 1")
            return [self.eval_1(si, active)]

    field = LegacyScalarField()
    si = surface_interaction()

    assert dr.allclose(mi.Field.eval(field, si), [0.5])
    assert dr.allclose(mi.Field.eval_1(field, si), 0.5)
    assert dr.allclose(mi.Field.eval_n(field, si), [0.5])
    assert dr.allclose(mi.Field.eval_n(field, si, 1), [0.5])

    with pytest.raises(RuntimeError, match="args_dim|expected 0|got 1"):
        mi.Field.eval_1(field, si, args=[1.0])


def test07_four_layer_scalar_field_nesting_has_exact_value(variant_scalar_rgb):
    register_nested_test_fields()

    field = mi.load_dict(
        affine_field(
            affine_field(
                affine_field({"type": "test_coordfield"}, 2.0, 1.0),
                -0.5,
                0.25,
            ),
            3.0,
            -2.0,
        )
    )
    si = surface_interaction()
    si.uv = mi.Point2f(0.2, 0.7)

    assert dr.allclose(field.eval_1(si), -10.25, atol=1e-6)


def test08_four_layer_nested_field_ad_matches_bilinear_weights(field_ad_rgb_variant):
    register_nested_test_fields()

    field = mi.load_dict(
        affine_field(
            affine_field(
                affine_field(scalar_bitmap_field([0.1, 0.2, 0.3, 0.4]),
                             2.0, 1.0),
                -1.5,
                -0.25,
            ),
            0.5,
            0.75,
        )
    )
    params = mi.traverse(field)
    dr.enable_grad(params["child.child.child.data"])
    params.update()

    si = surface_interaction()
    si.uv = mi.Point2f(0.5, 0.5)
    value = field.eval_1(si)
    assert dr.allclose(value, -0.5, atol=1e-6)

    dr.backward(value)
    expected_grad = dr.full(mi.TensorXf, -0.375, shape=(2, 2, 1))
    assert dr.allclose(dr.grad(params["child.child.child.data"]),
                       expected_grad, atol=1e-6)


def test09_checkerboard_scalar_eval_keeps_upstream_parity(variant_scalar_rgb):
    texture = mi.load_dict({
        "type": "checkerboard",
        "color0": 0.2,
        "color1": 0.8,
    })
    si = surface_interaction()

    si.uv = mi.Point2f(0.25, 0.25)
    assert dr.allclose(texture.eval(si)[0], 0.2)
    assert dr.allclose(texture.eval_1(si), 0.8)

    si.uv = mi.Point2f(0.75, 0.25)
    assert dr.allclose(texture.eval(si)[0], 0.8)
    assert dr.allclose(texture.eval_1(si), 0.2)


def test10_generic_only_python_field_uses_dynamic_fallback(variant_scalar_rgb):
    class GenericOnlyField(mi.Field):
        def __init__(self):
            mi.Field.__init__(self, mi.Properties("generic_only_field"))

        def out_type(self):
            return mi.FieldValueType.Float

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            return 1

        def args_dim(self):
            return 0

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

        def eval(self, si, args=None, active=True):
            return mi.ArrayXf([dr.select(active, si.uv.x + si.uv.y, 0.0)])

    field = GenericOnlyField()
    si = surface_interaction()

    assert dr.allclose(mi.Field.eval(field, si), [1.0])
    assert dr.allclose(mi.Field.eval_1(field, si), 1.0)
    assert dr.allclose(mi.Field.eval_n(field, si, 1), [1.0])
