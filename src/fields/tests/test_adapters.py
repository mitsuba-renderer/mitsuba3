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


def bitmap_field(value=(0.2, 0.4, 0.6)):
    return {
        "type": "bitmapfield",
        "data": mi.TensorXf(value, shape=(1, 1, len(value))),
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }


def grid_field(values):
    return {
        "type": "gridfield",
        "data": mi.TensorXf(values, shape=(1, 1, 1, len(values))),
        "raw": True,
        "filter_type": "nearest",
        "wrap_mode": "clamp",
    }


def fieldvolume(values, **kwargs):
    result = {
        "type": "fieldvolume",
        "field": grid_field(values),
    }
    result.update(kwargs)
    return result


def semantic_dim(out_type, features_dim=6):
    if out_type == mi.FieldValueType.Float:
        return 1
    if out_type == mi.FieldValueType.Spectrum:
        return dr.size_v(mi.UnpolarizedSpectrum)
    if out_type == mi.FieldValueType.Array2:
        return 2
    if out_type in (mi.FieldValueType.Color3, mi.FieldValueType.Array3):
        return 3
    return features_dim


def make_surface_field(out_type=None, args_dim=0, out_dim=None):
    class SurfaceField(mi.Field):
        def __init__(self):
            super().__init__(mi.Properties("surface_field"))

        def out_type(self):
            return out_type or mi.FieldValueType.Color3

        def domain(self):
            return mi.FieldDomain.Surface

        def out_dim(self):
            return out_dim or semantic_dim(self.out_type())

        def args_dim(self):
            return args_dim

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return False

        def eval_1(self, si, args=None, active=True):
            return dr.select(active, mi.Float(si.uv.x), 0.0)

        def eval_color3(self, si, args=None, active=True):
            return mi.Color3f(si.uv.x, si.uv.y, 0.25) & active

        def eval_array2(self, si, args=None, active=True):
            return mi.Point2f(si.uv.x, si.uv.y) & active

        def eval_spec(self, si, args=None, active=True):
            return mi.UnpolarizedSpectrum(self.eval_1(si, args=args,
                                                      active=active))

        def eval_n(self, si, count, args=None, active=True):
            if count != self.out_dim():
                raise RuntimeError("count must match out_dim")
            if self.out_type() == mi.FieldValueType.Float:
                return mi.ArrayXf([self.eval_1(si, args=args, active=active)])
            if self.out_type() == mi.FieldValueType.Array2:
                return mi.ArrayXf(self.eval_array2(si, args=args, active=active))
            if self.out_type() == mi.FieldValueType.Spectrum:
                return mi.ArrayXf(self.eval_spec(si, args=args, active=active))
            return mi.ArrayXf(self.eval_color3(si, args=args, active=active))

    return SurfaceField()


def make_interaction_field(out_type=None, args_dim=0, out_dim=None):
    class InteractionField(mi.Field):
        def __init__(self):
            super().__init__(mi.Properties("interaction_field"))

        def out_type(self):
            return out_type or mi.FieldValueType.Color3

        def domain(self):
            return mi.FieldDomain.Interaction

        def out_dim(self):
            return out_dim or semantic_dim(self.out_type())

        def args_dim(self):
            return args_dim

        def supports_scalar(self):
            return True

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return False

        def supports_interaction_queries(self):
            return True

        def eval_1(self, it, args=None, active=True):
            return dr.select(active, mi.Float(it.p.x), 0.0)

        def eval_color3(self, it, args=None, active=True):
            return mi.Color3f(it.p.x, it.p.y, it.p.z) & active

        def eval_array2(self, it, args=None, active=True):
            return mi.Point2f(it.p.x, it.p.y) & active

        def eval_array3(self, it, args=None, active=True):
            return mi.Vector3f(it.p.x, it.p.y, it.p.z) & active

        def eval_spec(self, it, args=None, active=True):
            return mi.UnpolarizedSpectrum(self.eval_1(it, args=args,
                                                      active=active))

        def eval_n(self, it, count, args=None, active=True):
            if count != self.out_dim():
                raise RuntimeError("count must match out_dim")
            if self.out_type() == mi.FieldValueType.Float:
                return mi.ArrayXf([self.eval_1(it, args=args, active=active)])
            if self.out_type() == mi.FieldValueType.Array2:
                return mi.ArrayXf(self.eval_array2(it, args=args, active=active))
            if self.out_type() == mi.FieldValueType.Array3:
                return mi.ArrayXf(self.eval_array3(it, args=args, active=active))
            if self.out_type() == mi.FieldValueType.Spectrum:
                return mi.ArrayXf(self.eval_spec(it, args=args, active=active))
            return mi.ArrayXf(self.eval_color3(it, args=args, active=active))

    return InteractionField()


def test01_adapter_plugins_register_as_texture_and_volume(variant_scalar_rgb):
    pmgr = mi.PluginManager.instance()

    assert pmgr.plugin_type("fieldtexture") == mi.ObjectType.Texture
    assert pmgr.plugin_type("fieldvolume") == mi.ObjectType.Volume


def test02_fieldtexture_eval_to_uv_metadata_and_diffuse(variant_scalar_rgb):
    transform = mi.ScalarTransform3f().translate([0.1, 0.2]).scale([2.0, 3.0])
    texture = mi.load_dict({
        "type": "fieldtexture",
        "field": make_surface_field(),
        "to_uv": transform,
        "mean_value": 0.42,
        "max_value": 0.9,
    })
    si = make_si(uv=(0.25, 0.5))
    uv = transform @ si.uv
    expected = mi.Color3f(uv.x, uv.y, 0.25)

    assert texture.is_spatially_varying()
    assert dr.allclose(texture.eval_3(si), expected)
    assert dr.allclose(texture.eval(si), expected)
    assert dr.allclose(texture.eval_1(si), mi.luminance(expected))
    assert dr.allclose(texture.mean(), 0.42)
    assert texture.max() == pytest.approx(0.9)

    scalar_texture = mi.load_dict({
        "type": "fieldtexture",
        "field": make_surface_field(out_type=mi.FieldValueType.Float),
    })
    scalar = scalar_texture.eval_1(si)
    assert dr.allclose(scalar_texture.eval_3(si), [scalar, scalar, scalar])

    params = mi.traverse(texture)
    assert "to_uv" in params
    assert "mean_value" in params
    assert "max_value" in params
    assert params["to_uv"] == transform

    reflectance = (0.2, 0.4, 0.6)
    bsdf = mi.load_dict({
        "type": "diffuse",
        "reflectance": {
            "type": "fieldtexture",
            "field": bitmap_field(reflectance),
            "mean_value": mi.luminance(mi.Color3f(*reflectance)),
            "max_value": max(reflectance),
        },
    })
    assert dr.allclose(bsdf.eval_diffuse_reflectance(si), reflectance)


def test03_fieldtexture_validation_and_unavailable_aggregates(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match="fieldtexture|Surface|Interaction"):
        mi.load_dict({
            "type": "fieldtexture",
            "field": grid_field([1.0, 2.0, 3.0]),
        })

    with pytest.raises(RuntimeError, match="fieldtexture|args_dim|0|1"):
        mi.load_dict({
            "type": "fieldtexture",
            "field": make_surface_field(args_dim=1),
        })

    with pytest.raises(RuntimeError, match="fieldtexture|Float|Color3|Spectrum|Array2"):
        mi.load_dict({
            "type": "fieldtexture",
            "field": make_surface_field(out_type=mi.FieldValueType.Array2),
        })

    texture = mi.load_dict({
        "type": "fieldtexture",
        "field": make_surface_field(),
    })
    with pytest.raises(RuntimeError, match="mean_value"):
        texture.mean()
    with pytest.raises(RuntimeError, match="max_value"):
        texture.max()
    with pytest.raises(RuntimeError, match="eval_1_grad|gradients"):
        texture.eval_1_grad(make_si())


def test04_fieldvolume_transform_max_and_texture_adapter(variant_scalar_rgb):
    to_world = (mi.ScalarTransform4f.translate([10.0, -2.0, 1.0]) @
                mi.ScalarTransform4f.scale([2.0, 4.0, 8.0]))
    volume = mi.load_dict({
        "type": "fieldvolume",
        "field": make_interaction_field(),
        "to_world": to_world,
        "max_value": 7.0,
    })
    world_it = make_it((11.0, 0.0, 5.0))
    expected = mi.Vector3f(0.5, 0.5, 0.5)

    assert volume.channel_count() == 3
    assert dr.all(volume.resolution() == [1, 1, 1])
    assert dr.allclose(volume.eval_3(world_it), expected)
    assert dr.allclose(volume.eval(world_it), expected)
    assert dr.allclose(volume.eval_1(world_it),
                       mi.luminance(mi.Color3f(expected.x, expected.y, expected.z)))
    assert dr.allclose(volume.eval_n(world_it), expected)
    assert volume.max() == 7.0
    assert dr.allclose(volume.max_per_channel(), [7.0, 7.0, 7.0])

    bbox = volume.bbox()
    assert dr.allclose(bbox.min, [10.0, -2.0, 1.0])
    assert dr.allclose(bbox.max, [12.0, 2.0, 9.0])

    texture = mi.load_dict({
        "type": "volume",
        "volume": volume,
    })
    si = make_si(p=(11.0, 0.0, 5.0))
    assert dr.allclose(texture.eval_3(si), expected)
    assert texture.max() == 7.0


def test05_fieldvolume_validation_majorant_and_sggx_consumers(variant_scalar_rgb):
    with pytest.raises(RuntimeError, match="fieldvolume|Interaction|Surface"):
        mi.load_dict({
            "type": "fieldvolume",
            "field": bitmap_field((0.5,)),
        })

    with pytest.raises(RuntimeError, match="fieldvolume|args_dim|0|1"):
        mi.load_dict({
            "type": "fieldvolume",
            "field": make_interaction_field(args_dim=1),
        })

    with pytest.raises(RuntimeError, match="fieldvolume|Float|Color3|Array3|Features|Array2"):
        mi.load_dict({
            "type": "fieldvolume",
            "field": make_interaction_field(out_type=mi.FieldValueType.Array2),
        })

    vector_volume = mi.load_dict(fieldvolume([1.0, 2.0, 3.0], max_value=3.0))
    assert dr.allclose(vector_volume.eval_3(make_it((0.5, 0.5, 0.5))),
                       [1.0, 2.0, 3.0])
    with pytest.raises(RuntimeError, match="fieldvolume::eval\\(\\)|spectrum|Array3"):
        vector_volume.eval(make_it())
    with pytest.raises(RuntimeError, match="fieldvolume::eval_6|Features\\[6\\]|Array3"):
        vector_volume.eval_6(make_it())

    spectrum_volume = mi.load_dict({
        "type": "fieldvolume",
        "field": make_interaction_field(out_type=mi.FieldValueType.Spectrum),
        "max_value": 1.0,
    })
    with pytest.raises(RuntimeError, match="fieldvolume::eval_3|Color3|Array3|Spectrum"):
        spectrum_volume.eval_3(make_it())

    volume_without_max = mi.load_dict({
        "type": "fieldvolume",
        "field": grid_field([0.5]),
    })
    with pytest.raises(RuntimeError, match="max_value"):
        volume_without_max.max()
    with pytest.raises(RuntimeError, match="max_value"):
        mi.load_dict({
            "type": "heterogeneous",
            "sigma_t": {
                "type": "fieldvolume",
                "field": grid_field([0.5]),
            },
            "albedo": 0.5,
        })

    medium = mi.load_dict({
        "type": "heterogeneous",
        "sigma_t": {
            "type": "fieldvolume",
            "field": grid_field([0.5]),
            "max_value": 0.5,
        },
        "albedo": 0.5,
    })
    assert medium is not None

    sggx_volume = fieldvolume([1.0, 1.0, 1.0, 0.0, 0.0, 0.0], max_value=1.0)
    volume = mi.load_dict(sggx_volume)
    assert volume.channel_count() == 6
    assert dr.allclose(volume.eval_6(make_it((0.5, 0.5, 0.5))),
                       [1.0, 1.0, 1.0, 0.0, 0.0, 0.0])

    phase = mi.load_dict({
        "type": "sggx",
        "S": sggx_volume,
    })
    mei = mi.MediumInteraction3f()
    mei.p = [0.5, 0.5, 0.5]
    mei.wi = [0, 0, 1]
    mei.sh_frame = mi.Frame3f([0, 0, 1])
    value, pdf = phase.eval_pdf(mi.PhaseFunctionContext(), mei, [0, 0, 1])
    assert dr.all(dr.isfinite(value))
    assert dr.all(dr.isfinite(pdf))


def test06_fieldvolume_gridfield_eval_n_and_traversal_keys(variant_scalar_rgb):
    texture = mi.load_dict({
        "type": "fieldtexture",
        "field": bitmap_field((0.25, 0.5, 0.75)),
        "mean_value": 0.5,
        "max_value": 0.75,
    })
    t_params = mi.traverse(texture)
    assert "field.data" in t_params
    assert "to_uv" in t_params
    assert "mean_value" in t_params
    assert "max_value" in t_params

    values = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0]
    volume = mi.load_dict(fieldvolume(values, max_value=6.0))
    it = make_it((0.5, 0.5, 0.5))
    assert dr.allclose(volume.eval_n(it), values)
    assert volume.channel_count() == 6

    v_params = mi.traverse(volume)
    assert "field.data" in v_params
    assert "max_value" in v_params

    with pytest.raises(RuntimeError, match="eval_gradient|gradients"):
        volume.eval_gradient(it)


def test07_adapter_field_params_receive_gradients(field_ad_rgb_variant):
    texture = mi.load_dict({
        "type": "fieldtexture",
        "field": {
            "type": "bitmapfield",
            "data": dr.zeros(mi.TensorXf, shape=(2, 2, 1)),
            "raw": True,
            "filter_type": "bilinear",
            "wrap_mode": "clamp",
        },
        "mean_value": 0.0,
        "max_value": 1.0,
    })
    t_params = mi.traverse(texture)
    assert "field.data" in t_params
    dr.enable_grad(t_params["field.data"])

    si = make_si(uv=(0.5, 0.5))
    dr.backward(texture.eval_1(si))
    t_grad = dr.grad(t_params["field.data"])
    assert dr.all(dr.isfinite(t_grad))
    assert dr.any(t_grad != 0)

    volume = mi.load_dict({
        "type": "fieldvolume",
        "field": {
            "type": "gridfield",
            "data": dr.zeros(mi.TensorXf, shape=(2, 2, 2, 1)),
            "raw": True,
            "filter_type": "trilinear",
            "wrap_mode": "clamp",
        },
        "max_value": 1.0,
    })
    v_params = mi.traverse(volume)
    assert "field.data" in v_params
    dr.enable_grad(v_params["field.data"])

    it = make_it((0.5, 0.5, 0.5))
    dr.backward(volume.eval_1(it))
    v_grad = dr.grad(v_params["field.data"])
    assert dr.all(dr.isfinite(v_grad))
    assert dr.any(v_grad != 0)


def test08_adapter_xml_loading_with_file_backed_fields(variant_scalar_rgb, tmpdir):
    bitmap_path = os.path.join(str(tmpdir), "field_texture.exr")
    bitmap_data = mi.TensorXf(
        [0.2, 0.4, 0.6] * 4,
        shape=(2, 2, 3)
    )
    mi.Bitmap(bitmap_data).write(bitmap_path)

    texture = mi.load_string(f"""<texture version="3.0.0" type="fieldtexture">
        <field name="field" type="bitmapfield">
            <string name="filename" value="{bitmap_path}"/>
            <boolean name="raw" value="true"/>
            <string name="filter_type" value="nearest"/>
            <string name="wrap_mode" value="clamp"/>
        </field>
    </texture>""")
    assert dr.allclose(texture.eval_3(make_si(uv=(0.5, 0.5))),
                       [0.2, 0.4, 0.6])

    volume_path = os.path.join(str(tmpdir), "field_volume.vol")
    volume_data = mi.TensorXf([0.5] * 8, shape=(2, 2, 2, 1))
    mi.VolumeGrid(volume_data).write(volume_path)

    volume = mi.load_string(f"""<volume version="3.0.0" type="fieldvolume">
        <float name="max_value" value="0.5"/>
        <field name="field" type="gridfield">
            <string name="filename" value="{volume_path}"/>
            <boolean name="raw" value="true"/>
            <string name="filter_type" value="nearest"/>
            <string name="wrap_mode" value="clamp"/>
        </field>
    </volume>""")
    assert dr.allclose(volume.eval_1(make_it((0.5, 0.5, 0.5))), 0.5)


def test09_adapter_neuralfield_nested_hashgrid_smoke(field_ad_rgb_variant):
    encoding_2d = {
        "type": "hashgridfield",
        "input_dim": 2,
        "n_levels": 2,
        "n_features_per_level": 2,
        "out_dim": 4,
        "hashmap_size": 16,
        "base_resolution": 2,
    }
    texture = mi.load_dict({
        "type": "fieldtexture",
        "field": {
            "type": "neuralfield",
            "domain": "Surface",
            "out_type": "Color3",
            "out_dim": 3,
            "args_dim": 0,
            "hidden_size": 4,
            "num_layers": 0,
            "encoding": encoding_2d,
        },
    })
    assert dr.all(dr.isfinite(texture.eval_3(make_si())))

    encoding_3d = dict(encoding_2d)
    encoding_3d["input_dim"] = 3
    volume = mi.load_dict({
        "type": "fieldvolume",
        "field": {
            "type": "neuralfield",
            "domain": "Interaction",
            "out_type": "Float",
            "out_dim": 1,
            "args_dim": 0,
            "hidden_size": 4,
            "num_layers": 0,
            "encoding": encoding_3d,
        },
        "max_value": 1.0,
    })
    assert dr.all(dr.isfinite(volume.eval_1(make_it())))
