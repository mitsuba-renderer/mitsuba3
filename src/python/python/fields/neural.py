from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import drjit.nn as nn

from .common import (
    _field_domain,
    _field_value_type,
    _field_value_type_name,
    _network_dtype,
    _require_jit,
    _semantic_dim,
    _surface_record,
    _validate_args,
    _vector_entries,
)


def _make_neural_field(mi):
    class NeuralField(mi.Field):
        def __init__(self, props):
            super().__init__(props)
            _require_jit(mi, "neuralfield")

            self._domain = _field_domain(mi, props.get("domain", "Surface"))
            self._out_type = _field_value_type(mi, props.get("out_type", "Color3"))
            self._out_dim = int(props.get("out_dim", 0))
            self._args_dim = int(props.get("args_dim", 0))
            self._decoder = str(props.get("decoder", "neural")).lower()
            self._hidden_size = int(props.get("hidden_size", 16))
            self._num_layers = int(props.get("num_layers", 2))

            if self._decoder not in ("neural", "linear"):
                raise RuntimeError(
                    'neuralfield: decoder must be "neural" or "linear".'
                )
            if self._hidden_size <= 0:
                raise RuntimeError("neuralfield: hidden_size must be positive.")
            if self._num_layers < 0:
                raise RuntimeError("neuralfield: num_layers must be non-negative.")

            expected_dim = _semantic_dim(mi, self._out_type)
            if self._out_type == mi.FieldValueType.Features:
                if self._out_dim <= 0:
                    raise RuntimeError(
                        "neuralfield: out_dim for Features outputs must be positive."
                    )
            elif self._out_dim == 0:
                self._out_dim = expected_dim
            elif self._out_dim != expected_dim:
                raise RuntimeError(
                    f"neuralfield: out_dim ({self._out_dim}) does not match "
                    f"metadata out_type={_field_value_type_name(mi, self._out_type)} "
                    f"(expected {expected_dim})."
                )

            self._encoding = props.get("encoding", None)
            if self._encoding is not None:
                if (
                    self._encoding.out_type() != mi.FieldValueType.Features
                    or self._encoding.out_dim() <= 0
                ):
                    raise RuntimeError(
                        "neuralfield: encoding must be a Features field with "
                        "positive out_dim."
                    )
                if self._encoding.args_dim() != 0:
                    raise RuntimeError(
                        "neuralfield: encoding fields may not require args."
                    )
                if (
                    self.supports_surface_queries()
                    and not self._encoding.supports_surface_queries()
                ):
                    raise RuntimeError(
                        "neuralfield: encoding field does not support Surface "
                        "queries required by the selected domain."
                    )
                if (
                    self.supports_interaction_queries()
                    and not self._encoding.supports_interaction_queries()
                ):
                    raise RuntimeError(
                        "neuralfield: encoding field does not support "
                        "Interaction queries required by the selected domain."
                    )

            tensor_dtype, scalar_dtype = _network_dtype(
                mi, props.get("precision", "fp16"), plugin_name="neuralfield"
            )
            self._feature_dim = (
                5
                + self._args_dim
                + (self._encoding.out_dim() if self._encoding is not None else 0)
            )

            layers = [nn.Cast(scalar_dtype)]
            if self._decoder == "linear":
                layers.append(nn.Linear(self._feature_dim, self._out_dim))
            else:
                in_features = self._feature_dim
                for _ in range(self._num_layers):
                    layers.append(nn.Linear(in_features, self._hidden_size))
                    layers.append(nn.LeakyReLU())
                    in_features = self._hidden_size
                layers.append(nn.Linear(in_features, self._out_dim))

            network = nn.Sequential(*layers).alloc(
                tensor_dtype,
                size=self._feature_dim,
                rng=dr.rng(seed=int(props.get("seed", 0))),
            )
            self.network_weights, self._network = nn.pack(
                network, layout="training"
            )
            self._network_weight_count = dr.width(self.network_weights)

        def out_type(self):
            return self._out_type

        def domain(self):
            return self._domain

        def out_dim(self):
            return self._out_dim

        def args_dim(self):
            return self._args_dim

        def supports_scalar(self):
            return False

        def supports_jit(self):
            return True

        def supports_surface_queries(self):
            return self._domain in (
                mi.FieldDomain.Surface,
                mi.FieldDomain.SurfaceAndInteraction,
            )

        def supports_interaction_queries(self):
            return self._domain in (
                mi.FieldDomain.Interaction,
                mi.FieldDomain.SurfaceAndInteraction,
            )

        def _validate_surface_query(self):
            if not self.supports_surface_queries():
                raise RuntimeError(
                    "neuralfield: domain mismatch, field does not support "
                    "Surface queries."
                )

        def _validate_interaction_query(self):
            if not self.supports_interaction_queries():
                raise RuntimeError(
                    "neuralfield: domain mismatch, field does not support "
                    "Interaction queries."
                )

        def _features(self, record, args, active):
            is_surface = _surface_record(record)
            if is_surface:
                self._validate_surface_query()
                uv_x, uv_y = record.uv.x, record.uv.y
            else:
                self._validate_interaction_query()
                uv_x, uv_y = mi.Float(0), mi.Float(0)

            _validate_args("neuralfield", self._args_dim, args)
            features = [
                record.p.x,
                record.p.y,
                record.p.z,
                uv_x,
                uv_y,
            ]
            if self._args_dim == 1 and not hasattr(args, "__getitem__"):
                features.append(args)
            else:
                features.extend(args[i] for i in range(self._args_dim))

            if self._encoding is not None:
                encoded = self._encoding.eval(record, active=active)
                features.extend(_vector_entries(encoded, self._encoding.out_dim()))

            return features

        def _eval_coop(self, record, args, active):
            return self._network(nn.CoopVec(*self._features(record, args, active)))

        def _validate_output(self, expected_type, expected_dim: int, method: str):
            if self._out_type != expected_type or self._out_dim != expected_dim:
                raise RuntimeError(
                    f"neuralfield::{method}(): incompatible output metadata "
                    f"(out_type={_field_value_type_name(mi, self._out_type)}, "
                    f"out_dim={self._out_dim})."
                )

        def eval(self, record, args=None, active=True):
            return mi.ArrayXf(self._eval_coop(record, args, active)) & active

        def eval_1(self, record, args=None, active=True):
            self._validate_output(mi.FieldValueType.Float, 1, "eval_1")
            result = list(self._eval_coop(record, args, active))
            return dr.select(active, mi.Float(result[0]), 0)

        def eval_color3(self, record, args=None, active=True):
            self._validate_output(mi.FieldValueType.Color3, 3, "eval_color3")
            return mi.Color3f(self._eval_coop(record, args, active)) & active

        def eval_array2(self, record, args=None, active=True):
            self._validate_output(mi.FieldValueType.Array2, 2, "eval_array2")
            result = list(self._eval_coop(record, args, active))
            return mi.Point2f(result[0], result[1]) & active

        def eval_array3(self, record, args=None, active=True):
            self._validate_output(mi.FieldValueType.Array3, 3, "eval_array3")
            result = list(self._eval_coop(record, args, active))
            return mi.Vector3f(result[0], result[1], result[2]) & active

        def eval_spec(self, record, args=None, active=True):
            expected = dr.size_v(mi.UnpolarizedSpectrum)
            self._validate_output(mi.FieldValueType.Spectrum, expected, "eval_spec")
            return (
                mi.UnpolarizedSpectrum(self._eval_coop(record, args, active))
                & active
            )

        def eval_array6(self, record, args=None, active=True):
            self._validate_output(mi.FieldValueType.Features, 6, "eval_array6")
            return mi.ArrayXf(self._eval_coop(record, args, active)) & active

        def eval_n(self, record, count, args=None, active=True):
            if int(count) != self._out_dim:
                raise RuntimeError(
                    f"neuralfield::eval_n(): count ({int(count)}) must match "
                    f"out_dim ({self._out_dim})."
                )
            return self.eval(record, args=args, active=active)

        def mean(self):
            return mi.Float(0.5)

        def traverse(self, cb):
            cb.put(
                "network_weights",
                self.network_weights,
                mi.ParamFlags.Differentiable,
            )
            if self._encoding is not None:
                cb.put("encoding", self._encoding, mi.ParamFlags.Differentiable)

        def parameters_changed(self, keys=None):
            if dr.width(self.network_weights) != self._network_weight_count:
                raise RuntimeError(
                    "neuralfield: network_weights size does not match metadata."
                )

        def to_string(self):
            return (
                "NeuralField[\n"
                f"  out_dim = {self._out_dim},\n"
                f"  args_dim = {self._args_dim},\n"
                f'  decoder = "{self._decoder}"\n'
                "]"
            )

    return NeuralField
