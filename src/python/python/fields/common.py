from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr


def _is_jit_variant(mi) -> bool:
    variant = mi.variant()
    return variant is not None and variant.startswith(("llvm_", "cuda_"))


def _require_jit(mi, plugin_name: str) -> None:
    if not _is_jit_variant(mi):
        raise RuntimeError(
            f'{plugin_name}: variant "{mi.variant()}" is unsupported. '
            "This field requires an LLVM or CUDA JIT variant."
        )


def _args_len(args) -> int:
    if args is None:
        return 0
    try:
        return len(args)
    except TypeError:
        return 1


def _validate_args(plugin_name: str, expected: int, args) -> None:
    actual = _args_len(args)
    if actual != expected:
        raise RuntimeError(
            f"{plugin_name}: args_dim mismatch (expected {expected}, got {actual})."
        )


def _field_value_type(mi, value: str):
    mapping = {
        "Float": mi.FieldValueType.Float,
        "Spectrum": mi.FieldValueType.Spectrum,
        "Color3": mi.FieldValueType.Color3,
        "Array2": mi.FieldValueType.Array2,
        "Array3": mi.FieldValueType.Array3,
        "Features": mi.FieldValueType.Features,
    }
    try:
        return mapping[str(value)]
    except KeyError:
        raise RuntimeError(f'Invalid field output type "{value}".') from None


def _field_domain(mi, value: str):
    mapping = {
        "Surface": mi.FieldDomain.Surface,
        "Interaction": mi.FieldDomain.Interaction,
        "SurfaceAndInteraction": mi.FieldDomain.SurfaceAndInteraction,
    }
    try:
        return mapping[str(value)]
    except KeyError:
        raise RuntimeError(f'Invalid field domain "{value}".') from None


def _semantic_dim(mi, out_type) -> int:
    if out_type == mi.FieldValueType.Float:
        return 1
    if out_type == mi.FieldValueType.Spectrum:
        return dr.size_v(mi.UnpolarizedSpectrum)
    if out_type == mi.FieldValueType.Color3:
        return 3
    if out_type == mi.FieldValueType.Array2:
        return 2
    if out_type == mi.FieldValueType.Array3:
        return 3
    return 0


def _field_value_type_name(mi, out_type) -> str:
    for name in ("Float", "Spectrum", "Color3", "Array2", "Array3", "Features"):
        if out_type == getattr(mi.FieldValueType, name):
            return name
    return "Unknown"


def _network_dtype(mi, precision: str, *, plugin_name: str):
    precision = str(precision).lower()
    if precision == "fp16":
        return mi.TensorXf16, mi.Float16
    if precision == "fp32":
        if plugin_name == "neuralfield" and mi.variant().startswith("cuda_"):
            raise RuntimeError(
                "neuralfield: precision=\"fp32\" is unsupported on CUDA "
                "CoopVec backends; use precision=\"fp16\"."
            )
        return mi.TensorXf, mi.Float
    raise RuntimeError(f'{plugin_name}: precision must be "fp16" or "fp32".')


def _surface_record(record) -> bool:
    return hasattr(record, "uv")


def _vector_entries(value, count: int):
    return [value[i] for i in range(count)]


def _make_drjit_feature_field(
    mi,
    *,
    plugin_name: str,
    encoding_cls,
    class_name: str,
    torchngp_compat: bool = False,
):
    class DrJitFeatureField(mi.Field):
        def __init__(self, props):
            super().__init__(props)
            _require_jit(mi, plugin_name)

            if "encoding" in props:
                raise RuntimeError(
                    f"{plugin_name}: nested encoding child composition is not "
                    "supported; compose encodings in neuralfield instead."
                )

            self._input_dim = int(props.get("input_dim", 2))
            if self._input_dim not in (2, 3):
                raise RuntimeError(f"{plugin_name}: input_dim must be 2 or 3.")

            self._n_levels = int(props.get("n_levels", 16))
            self._n_features_per_level = int(props.get("n_features_per_level", 2))
            self._out_dim = int(
                props.get("out_dim", self._n_levels * self._n_features_per_level)
            )
            natural_dim = self._n_levels * self._n_features_per_level
            if self._out_dim != natural_dim:
                raise RuntimeError(
                    f"{plugin_name}: out_dim must equal "
                    "n_levels * n_features_per_level "
                    f"({natural_dim}), got {self._out_dim}."
                )

            tensor_dtype, _ = _network_dtype(
                mi, props.get("precision", "fp16"), plugin_name=plugin_name
            )

            kwargs = {
                "n_levels": self._n_levels,
                "n_features_per_level": self._n_features_per_level,
                "hashmap_size": int(props.get("hashmap_size", 2**19)),
                "base_resolution": int(props.get("base_resolution", 16)),
                "per_level_scale": float(props.get("per_level_scale", 2.0)),
                "align_corners": bool(props.get("align_corners", False)),
                "smooth_weight_gradients": bool(
                    props.get("smooth_weight_gradients", False)
                ),
                "smooth_weight_lambda": float(
                    props.get("smooth_weight_lambda", 1.0)
                ),
                "init_scale": float(props.get("init_scale", 1e-4)),
                "rng": dr.rng(seed=int(props.get("seed", 0))),
            }
            if torchngp_compat:
                kwargs["torchngp_compat"] = bool(
                    props.get("torchngp_compat", False)
                )

            self._encoding = encoding_cls(tensor_dtype, self._input_dim, **kwargs)

        def out_type(self):
            return mi.FieldValueType.Features

        def domain(self):
            return mi.FieldDomain.SurfaceAndInteraction

        def out_dim(self):
            return self._out_dim

        def args_dim(self):
            return 0

        def supports_scalar(self):
            return False

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return True

        def supports_interaction_queries(self):
            return self._input_dim == 3

        def _coords(self, record):
            if self._input_dim == 2:
                if not _surface_record(record):
                    raise RuntimeError(
                        f"{plugin_name}: Interaction queries require input_dim=3."
                    )
                return (record.uv.x, record.uv.y)
            return (record.p.x, record.p.y, record.p.z)

        def eval(self, record, args=None, active=True):
            _validate_args(plugin_name, 0, args)
            return mi.ArrayXf(self._encoding(self._coords(record), active))

        def eval_n(self, record, count, args=None, active=True):
            if int(count) != self._out_dim:
                raise RuntimeError(
                    f"{plugin_name}::eval_n(): count ({int(count)}) must "
                    f"match out_dim ({self._out_dim})."
                )
            return self.eval(record, args=args, active=active)

        def traverse(self, cb):
            cb.put("params", self._encoding.data, mi.ParamFlags.Differentiable)

        def parameters_changed(self, keys=None):
            if dr.width(self._encoding.data) != self._encoding.n_params:
                raise RuntimeError(
                    f"{plugin_name}: parameter size must match the Dr.Jit "
                    "encoding layout."
                )

        def to_string(self):
            return (
                f"{class_name}[\n"
                f"  input_dim = {self._input_dim},\n"
                f"  out_dim = {self._out_dim}\n"
                "]"
            )

    DrJitFeatureField.__name__ = class_name
    DrJitFeatureField.__qualname__ = class_name
    return DrJitFeatureField
