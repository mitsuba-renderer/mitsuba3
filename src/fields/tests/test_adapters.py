import os

import pytest
import drjit as dr
import mitsuba as mi


def make_si(p=(0.25, 0.5, 0.0), uv=(0.25, 0.5)):
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.p = mi.Point3f(*p)
    si.uv = mi.Point2f(*uv)
    si.n = mi.Normal3f(0, 0, 1)
    si.wi = mi.Vector3f(0, 0, 1)
    si.sh_frame = mi.Frame3f(si.n)
    return si


def make_it(p=(0.25, 0.5, 0.75)):
    it = dr.zeros(mi.Interaction3f)
    it.p = mi.Point3f(*p)
    return it


def bitmap_field(value=(0.2, 0.4, 0.6), **kwargs):
    result = {
        "type": "bitmap",
        "data": mi.TensorXf(value, shape=(1, 1, len(value))),
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }
    result.update(kwargs)
    return result


def grid_field(values, **kwargs):
    result = {
        "type": "gridvolume",
        "data": mi.TensorXf(values, shape=(1, 1, 1, len(values))),
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }
    result.update(kwargs)
    return result


def test01_texture_and_field_tags_can_instantiate_bitmap(variant_scalar_rgb, tmpdir):
    value = (0.2, 0.4, 0.6)
    texture = mi.load_dict(bitmap_field(value))
    assert isinstance(texture, mi.Field)
    field = texture.field()
    assert field is texture
    assert texture.domain() == mi.FieldDomain.Surface
    assert texture.out_type() == mi.FieldValueType.Color3
    assert dr.allclose(texture.eval_3(make_si()), value)
    assert dr.allclose(field.eval_color3(make_si()), value)
    assert dr.allclose(mi.Field.eval(field, make_si())[0], value[0])

    bitmap_path = os.path.join(str(tmpdir), "reflectance.exr")
    mi.Bitmap(mi.TensorXf(list(value) * 4, shape=(2, 2, 3))).write(bitmap_path)

    xml = f"""<scene version="3.0.0">
        <shape type="rectangle">
            <bsdf type="diffuse">
                <texture name="reflectance" type="bitmap">
                    <string name="filename" value="{bitmap_path}"/>
                    <boolean name="raw" value="true"/>
                    <string name="filter_type" value="nearest"/>
                    <string name="wrap_mode" value="clamp"/>
                </texture>
            </bsdf>
        </shape>
    </scene>"""
    scene = mi.load_string(xml)
    bsdf = scene.shapes()[0].bsdf()
    assert dr.allclose(bsdf.eval_diffuse_reflectance(make_si()), value)


def test02_volume_and_field_tags_can_instantiate_gridvolume(variant_scalar_rgb, tmpdir):
    values = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0]
    volume = mi.load_dict(grid_field(values, max_value=6.0))
    assert isinstance(volume, mi.Field)
    field = volume.field()
    assert field is volume
    assert volume.domain() == mi.FieldDomain.Interaction
    assert volume.out_type() == mi.FieldValueType.Features
    assert volume.out_dim() == 6
    assert volume.channel_count() == 6
    assert dr.allclose(volume.eval_6(make_it((0.5, 0.5, 0.5))), values)
    assert dr.allclose(field.eval_array6(make_it((0.5, 0.5, 0.5))), values)
    assert dr.allclose(mi.Field.eval_n(field, make_it((0.5, 0.5, 0.5)), 6),
                       values)

    volume_path = os.path.join(str(tmpdir), "field_volume.vol")
    volume_data = mi.TensorXf(values * 8, shape=(2, 2, 2, 6))
    mi.VolumeGrid(volume_data).write(volume_path)

    xml = f"""<field version="3.0.0" type="gridvolume">
        <string name="filename" value="{volume_path}"/>
        <boolean name="raw" value="true"/>
        <string name="filter_type" value="nearest"/>
        <string name="wrap_mode" value="clamp"/>
        <float name="max_value" value="6"/>
    </field>"""
    xml_volume = mi.load_string(xml)
    assert isinstance(xml_volume, mi.Field)
    assert dr.allclose(xml_volume.eval_array6(make_it((0.5, 0.5, 0.5))),
                       values)


def test03_role_tags_still_reject_incompatible_field_implementations(variant_scalar_rgb, tmpdir):
    volume_path = os.path.join(str(tmpdir), "role_volume.vol")
    mi.VolumeGrid(mi.TensorXf([1.0] * 8, shape=(2, 2, 2, 1))).write(volume_path)

    with pytest.raises(RuntimeError, match="Type mismatch|texture|field|gridvolume"):
        mi.load_string(f"""<texture version="3.0.0" type="gridvolume">
            <string name="filename" value="{volume_path}"/>
            <boolean name="raw" value="true"/>
        </texture>""")

    bitmap_path = os.path.join(str(tmpdir), "role_texture.exr")
    mi.Bitmap(mi.TensorXf([0.2, 0.4, 0.6] * 4, shape=(2, 2, 3))).write(bitmap_path)

    with pytest.raises(RuntimeError, match="Type mismatch|volume|field|bitmap"):
        mi.load_string(f"""<volume version="3.0.0" type="bitmap">
            <string name="filename" value="{bitmap_path}"/>
            <boolean name="raw" value="true"/>
        </volume>""")

    with pytest.raises(RuntimeError, match="Texture role|Features\\[8\\]"):
        mi.load_string("""<texture version="3.0.0" type="sinusoidalfield">
            <integer name="input_dim" value="2"/>
            <integer name="out_dim" value="8"/>
        </texture>""")

    with pytest.raises(RuntimeError, match="Volume role|Features\\[8\\]"):
        mi.load_string("""<volume version="3.0.0" type="sinusoidalfield">
            <integer name="input_dim" value="3"/>
            <integer name="out_dim" value="8"/>
        </volume>""")

    texture_like_field = mi.load_string(
        """<field version="3.0.0" type="sinusoidalfield">
            <integer name="input_dim" value="2"/>
            <integer name="out_dim" value="8"/>
        </field>"""
    )
    volume_like_field = mi.load_string(
        """<field version="3.0.0" type="sinusoidalfield">
            <integer name="input_dim" value="3"/>
            <integer name="out_dim" value="8"/>
        </field>"""
    )
    assert isinstance(texture_like_field, mi.Field)
    assert isinstance(volume_like_field, mi.Field)


def test04_direct_field_traversal_has_no_adapter_prefix(field_ad_rgb_variant):
    texture = mi.load_dict(bitmap_field((0.25,), data=dr.zeros(mi.TensorXf, shape=(2, 2, 1)),
                                        filter_type="bilinear"))
    t_params = mi.traverse(texture)
    assert "data" in t_params
    assert "to_uv" in t_params
    assert not any(key.startswith("field.") for key in t_params.keys())

    dr.enable_grad(t_params["data"])
    t_params.update()
    si = make_si(uv=(0.5, 0.5))
    dr.backward(texture.eval_1(si))
    assert dr.any(dr.grad(t_params["data"]) != 0)

    volume = mi.load_dict(grid_field([0.0], data=dr.zeros(mi.TensorXf, shape=(2, 2, 2, 1)),
                                     filter_type="trilinear"))
    v_params = mi.traverse(volume)
    assert "data" in v_params
    assert not any(key.startswith("field.") for key in v_params.keys())

    dr.enable_grad(v_params["data"])
    v_params.update()
    it = make_it((0.5, 0.5, 0.5))
    dr.backward(volume.eval_1(it))
    assert dr.any(dr.grad(v_params["data"]) != 0)


def test05_file_backed_bitmap_and_gridvolume_work_as_fields(variant_scalar_rgb, tmpdir):
    bitmap_path = os.path.join(str(tmpdir), "field_texture.exr")
    bitmap_data = mi.TensorXf([0.2, 0.4, 0.6] * 4, shape=(2, 2, 3))
    mi.Bitmap(bitmap_data).write(bitmap_path)

    texture = mi.load_string(f"""<field version="3.0.0" type="bitmap">
        <string name="filename" value="{bitmap_path}"/>
        <boolean name="raw" value="true"/>
        <string name="filter_type" value="nearest"/>
        <string name="wrap_mode" value="clamp"/>
    </field>""")
    assert isinstance(texture, mi.Field)
    assert dr.allclose(texture.eval_color3(make_si(uv=(0.5, 0.5))),
                       [0.2, 0.4, 0.6])

    volume_path = os.path.join(str(tmpdir), "field_volume.vol")
    volume_data = mi.TensorXf([0.5] * 8, shape=(2, 2, 2, 1))
    mi.VolumeGrid(volume_data).write(volume_path)

    volume = mi.load_string(f"""<field version="3.0.0" type="gridvolume">
        <string name="filename" value="{volume_path}"/>
        <boolean name="raw" value="true"/>
        <string name="filter_type" value="nearest"/>
        <string name="wrap_mode" value="clamp"/>
    </field>""")
    assert isinstance(volume, mi.Field)
    assert dr.allclose(volume.eval_1(make_it((0.5, 0.5, 0.5))), 0.5)


def test06_python_fields_are_not_volume_role_implementations(variant_scalar_rgb):
    assert not hasattr(mi, "Volume")

    class PythonUnitField(mi.Field):
        def __init__(self, props):
            mi.Field.__init__(self, props)

        def out_type(self):
            return mi.FieldValueType.Float

        def domain(self):
            return mi.FieldDomain.Interaction

        def out_dim(self):
            return 1

        def args_dim(self):
            return 0

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return False

        def supports_interaction_queries(self):
            return True

        def eval_1(self, it, args=None, active=True):
            return dr.select(active, 0.75, 0.0)

        def max(self):
            return 0.75

    try:
        mi.register_field("python_unit_field", PythonUnitField)
    except RuntimeError as exc:
        if "already" not in str(exc).lower():
            raise

    field = mi.load_string("""<field version="3.0.0" type="python_unit_field"/>""")
    assert isinstance(field, mi.Field)
    assert dr.allclose(field.eval_1(make_it()), 0.75)

    with pytest.raises(RuntimeError, match="Volume role requires a VolumeField"):
        mi.load_string("""<volume version="3.0.0" type="python_unit_field"/>""")
