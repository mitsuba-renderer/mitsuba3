import pytest
import drjit as dr
import mitsuba as mi
import subprocess
import sys


config = mi.parser.ParserConfig("scalar_rgb")


def make_metadata_only_field():
    class MetadataOnlyField(mi.Field):
        def __init__(self):
            super().__init__(mi.Properties("metadata_only_field"))

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
            return False

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

    return MetadataOnlyField()


def test01_field_enums_are_not_registered_per_variant():
    code = """
import mitsuba as mi
variants = ['scalar_rgb']
if 'llvm_ad_rgb' in mi.variants():
    variants.append('llvm_ad_rgb')
for variant in variants:
    mi.set_variant(variant)
    assert mi.FieldValueType.Color3.name == 'Color3'
    assert mi.FieldDomain.Surface.name == 'Surface'
"""
    subprocess.run([sys.executable, "-c", code], check=True)


def test02_xml_field_tag_is_typed_and_round_trips(variant_scalar_rgb):
    xml = """<scene version="3.0.0">
        <field type="debugfield" id="f0">
            <integer name="out_dim" value="3"/>
        </field>
    </scene>"""

    state = mi.parser.parse_string(config, xml)
    field_nodes = [n for n in state.nodes if n.type == mi.ObjectType.Field]

    assert len(field_nodes) == 1
    assert field_nodes[0].props.plugin_name() == "debugfield"
    assert field_nodes[0].props.id() == "f0"
    assert field_nodes[0].props["out_dim"] == 3

    output = mi.parser.write_string(state)
    assert "<field" in output
    assert 'type="debugfield"' in output
    assert 'name="out_dim" value="3"' in output

    state_rt = mi.parser.parse_string(config, output)
    field_nodes_rt = [n for n in state_rt.nodes if n.type == mi.ObjectType.Field]
    assert len(field_nodes_rt) == 1
    assert field_nodes_rt[0].props.plugin_name() == "debugfield"
    assert field_nodes_rt[0].props["out_dim"] == 3


def test03_python_field_metadata_dispatches_through_virtual_api(variant_scalar_rgb):
    field = make_metadata_only_field()

    assert field.out_type() == mi.FieldValueType.Color3
    assert field.domain() == mi.FieldDomain.Surface
    assert field.out_dim() == 3
    assert field.args_dim() == 4
    assert field.supports_scalar()
    assert not field.supports_jit()
    assert field.supports_surface_queries()
    assert not field.supports_interaction_queries()


@pytest.mark.parametrize(
    "name, call",
    [
        ("eval", lambda f, si: f.eval(si)),
        ("eval_1", lambda f, si: f.eval_1(si)),
        ("eval_color3", lambda f, si: f.eval_color3(si)),
        ("eval_array2", lambda f, si: f.eval_array2(si)),
        ("eval_array3", lambda f, si: f.eval_array3(si)),
        ("eval_spec", lambda f, si: f.eval_spec(si)),
        ("eval_array6", lambda f, si: f.eval_array6(si)),
        ("eval_n", lambda f, si: f.eval_n(si, 3)),
    ],
)
def test04_surface_evaluation_stubs_raise_specific_errors(variant_scalar_rgb, name, call):
    si = dr.zeros(mi.SurfaceInteraction3f)

    with pytest.raises(RuntimeError, match=name):
        call(make_metadata_only_field(), si)


@pytest.mark.parametrize(
    "name, call",
    [
        ("eval", lambda f, it: f.eval(it)),
        ("eval_1", lambda f, it: f.eval_1(it)),
        ("eval_color3", lambda f, it: f.eval_color3(it)),
        ("eval_array2", lambda f, it: f.eval_array2(it)),
        ("eval_array3", lambda f, it: f.eval_array3(it)),
        ("eval_spec", lambda f, it: f.eval_spec(it)),
        ("eval_array6", lambda f, it: f.eval_array6(it)),
        ("eval_n", lambda f, it: f.eval_n(it, 6)),
    ],
)
def test05_interaction_evaluation_stubs_raise_specific_errors(variant_scalar_rgb, name, call):
    it = dr.zeros(mi.Interaction3f)

    with pytest.raises(RuntimeError, match=name):
        call(make_metadata_only_field(), it)


def test06_texture_and_volume_public_apis_do_not_accept_field_args(variant_scalar_rgb):
    tex = mi.load_dict({
        "type": "bitmap",
        "data": dr.ones(mi.TensorXf, shape=[2, 2, 3]),
        "raw": True,
    })
    vol = mi.load_dict({
        "type": "gridvolume",
        "data": dr.ones(mi.TensorXf, shape=[2, 2, 2, 1]),
        "raw": True,
    })

    si = dr.zeros(mi.SurfaceInteraction3f)
    it = dr.zeros(mi.Interaction3f)

    with pytest.raises(TypeError):
        tex.eval(si, args=[1.0, 2.0])
    with pytest.raises(TypeError):
        tex.eval_1(si, args=[1.0, 2.0])
    with pytest.raises(TypeError):
        vol.eval(it, args=[1.0, 2.0])
    with pytest.raises(TypeError):
        vol.eval_n(it, args=[1.0, 2.0])


def test07_no_public_constant_or_coordinate_field_plugins(variant_scalar_rgb):
    pmgr = mi.PluginManager.instance()

    assert pmgr.plugin_type("constantfield") == mi.ObjectType.Unknown
    assert pmgr.plugin_type("coordfield") == mi.ObjectType.Unknown


def test08_fieldptr_is_bound_for_jit_variants(variant_llvm_ad_rgb):
    assert hasattr(mi, "FieldPtr")

    for name in [
        "eval",
        "eval_1",
        "eval_color3",
        "eval_array2",
        "eval_array3",
        "eval_spec",
        "eval_array6",
    ]:
        assert hasattr(mi.FieldPtr, name)
