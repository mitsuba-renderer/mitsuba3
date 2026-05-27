# Field Refactor Review

Scope: reviewed the current `field` branch against `master...HEAD` and the upstream checkout in `mitsuba-std/`. I also reviewed the dirty working tree state that existed before this pass and applied narrow fixes where the issue was correctness-critical and low risk.

Build and verification used the configured `build-gpt-5` tree from `LLM.md` with scalar and CUDA variants available.

## Fixes Applied In This Pass

- `src/core/parser.cpp`: `transform_merge_equivalent()` now skips `ObjectType::Field`. `Properties::hash()`/equality ignore object identity, so identical direct fields could previously be merged and would share state or optimization parameters incorrectly.
- `include/mitsuba/core/plugin.h`, `src/core/plugin.cpp`, `src/core/python/object.cpp`, `src/core/python/parser.cpp`: `PluginManager::plugin_type()` now has a variant-aware overload, and Python `parse_dict()` uses `ParserConfig.variant`. This fixes JIT-only Python field plugins being parsed as `Unknown` under non-default variants.
- `src/python/__init__.py`: Python `Field` subclass wrappers now recognize old positional `active` calls such as `field.eval_1(si, True)` when the override also has an `args=` parameter.
- `benchmarks/fields/bench_fields.py`: restored standard volume benchmark compatibility by using `gridvolume` for the volume baseline; added `--out-type`/auto output metadata so `neural_field_inference --method eval_1 --out-dim 1` constructs `Float[1]` instead of `Features[1]`.
- `src/fields/tests/test_field.py`: added regression tests for field deduplication, variant-aware dictionary parsing, and positional `active` handling.

Verification after the fixes:

- `ninja -C build-gpt-5`: passed.
- `python -m pytest -q src/fields/tests/test_field.py src/render/tests/test_texture.py src/python_tests/test_util.py`: `30 passed`.
- `python -m pytest -q src/fields/tests/test_neural.py src/bsdfs/tests/test_neuralbsdf.py`: `33 passed, 2 skipped`.
- `python -m pytest -q src/fields/tests`: `180 passed`.
- Scalar benchmark smoke: `grid_volume_eval --variant scalar_rgb --method eval_1 --channels 1`: passed.
- CUDA benchmark smoke: `neural_field_inference --variant cuda_ad_rgb --method eval_1 --out-dim 1 --args-mode no_args`: passed.

## Remaining High-Priority Review Findings

1. `include/mitsuba/render/field.h` keeps both `domain()` and `supports_surface_queries()`/`supports_interaction_queries()` (`field.h:94-113`). These can contradict each other, and role validation uses the booleans. Prefer one source of truth, likely deriving the two support predicates from `FieldDomain`, unless a concrete field needs an exception.

2. The `require_field_*()` helpers execute methods during construction (`field.h:382-430`). For JIT or learned fields this can trace, allocate, force kernels, or touch state just to validate capability. Replace these probes with explicit capability metadata or move validation to the first actual consumer call.

3. `SurfaceField` and `VolumeField` duplicate a large amount of adapter/fallback logic (`src/render/texture.cpp:58-190`, `src/render/volume.cpp:57-190`). The compatibility layer is useful during migration, but the current shape keeps old texture/volume semantics embedded in the new generic base. Simplify once the role boundary is stable.

4. Texture/volume role instantiation validates twice: role object creation happens in `create_texture_role_object_for_variant()`/`create_volume_role_object_for_variant()`, then `instantiate()` expands and runs `make_*_object_for_variant()` again (`src/core/parser.cpp:1718-1759`). It is not a hot path, but it is extra complexity.

5. `neuralbsdf` is named as if it consumes direct neural fields, but its diffuse mode is texture-role-like: it calls `get_surface_field<Field>()` and rejects `args_dim()!=0` (`src/bsdfs/neuralbsdf.cpp:59-66`), then always evaluates with empty args (`src/bsdfs/neuralbsdf.cpp:217-234`). If the goal is a direct contextual field BSDF, this needs a different API. If the goal is diffuse albedo from fields, the current implementation is coherent but the name is broader than the behavior.

6. BSDF attribute lookup now validates field metadata on every query (`src/render/bsdf.cpp:62-68`). This is probably acceptable for AOV/debug paths, but construction-time validation or cached dispatch would avoid virtual metadata calls in repeated attribute evaluation.

7. API docs are incomplete. `docs/docs_api/conf.py` and `docs/docs_api/list_api.rst` remove old Texture/Volume API entries without adding a `Field`/`FieldPtr`/`register_field` section. Plugin docs still route selected `src/fields` files through legacy texture/volume sections, which is fine temporarily but not a complete Field API story.

8. Test coverage is biased toward CUDA when both JIT backends are available (`src/fields/tests/conftest.py:10-28`), and several tests are CUDA-only despite the code claiming LLVM support. Add explicit LLVM runs for neural/hashgrid/permuto and field role consumers before relying on backend parity.

## Performance Notes

- The hot texture and volume plugins (`bitmap`, `gridvolume`) still preserve direct fixed-output paths for role consumers. That is the right performance shape: old BSDF/emitter/media code calls `eval_1`, `eval_3`, `eval`, etc., and does not have to allocate `DynamicArray` unless it chooses the generic `Field::eval()`/`eval_n()` interface.
- Generic `Field::eval()` necessarily allocates dynamic storage and should not silently replace fixed role calls in CUDA hot paths. Keep generic evaluation for feature fields, neural fields, and introspection.
- Construction-time method probing is the main avoidable CUDA/JIT overhead risk found in this pass.
- The benchmark harness is now able to exercise scalar and CUDA fixed-output neural cases. Use it to compare kernel launches and memory watermark before considering the refactor performance-complete.

## File-by-File Review

### Build, Docs, And Benchmarks

- `.gitignore`: benchmark/output hygiene only. Not functionally relevant to the refactor.
- `benchmarks/fields/README.md`: useful local benchmark documentation; keep out of final public docs unless benchmark scripts become supported tooling.
- `benchmarks/fields/bench_fields.py`: necessary for measuring refactor overhead. I fixed the upstream volume plugin name and neural output metadata issue. Remaining caveat: metadata reports `build-{variant}` unless overridden by env, while this workspace uses `build-gpt-5`.
- `benchmarks/fields/compare_field_vs_std.py` (untracked): useful comparison script. Its dropping of missing standard rows was masking the `grid`/`gridvolume` issue; with the benchmark fix it should produce the intended volume comparisons.
- `docs/docs_api/conf.py`: removes old Texture class docs but does not add Field docs. Needs a Field category.
- `docs/docs_api/list_api.rst`: removes Texture/Volume/register_texture entries but misses Field/register_field replacements.
- `docs/generate_plugin_doc.py`: necessary transitional change to generate texture/volume plugin docs from `src/fields`. The include filter is simple enough and preferable to duplicating plugin source trees.
- `src/CMakeLists.txt`: swaps old texture/volume plugin directories for `fields`; necessary build integration.

### Core API, Parser, And Properties

- `include/mitsuba/core/object.h`: adds `ObjectType::Field`; necessary and straightforward.
- `include/mitsuba/core/plugin.h`: variant-aware `plugin_type()` is necessary for Python/JIT fields and dictionary parser correctness.
- `src/core/plugin.cpp`: variant-aware lookup is fixed. Existing no-variant overload retains old default-variant behavior.
- `src/core/python/object.cpp`: exposes optional variant lookup in Python; useful for tests and parser tooling.
- `include/mitsuba/core/properties.h`: adds surface/volume field retrieval APIs plus compatibility `get_texture()` spellings. Functionally necessary, but the public compatibility layer is larger than ideal; keep it only if C++ compatibility is an explicit goal.
- `src/core/properties.cpp`: role conversion helpers are the right central place for texture/volume compatibility. The validation logic is thorough. Simplification target: avoid duplicate validation and avoid relying on method probes for capability checks.
- `src/core/parser.cpp`: XML `<field>` support and field ordering/relocation are necessary. I fixed field deduplication. The role conversion flow still has double validation.
- `src/core/python/parser.cpp`: dictionary parser now uses `ParserConfig.variant`. The rest of the dictionary parsing changes are structurally appropriate.
- `src/core/tests/test_parser.py`: good parser coverage for field tags/role compatibility. Keep.
- `src/core/tests/test_properties.py`: small role conversion coverage; adequate for a narrow migration test.

### Public Headers And Render Base Classes

- `include/mitsuba/render/field.h`: central new API. The generic/fixed split is useful, but `domain()` vs support booleans and construction-time `require_field_*()` probes should be simplified before this becomes stable API.
- `src/render/field.cpp`: implements metadata defaults and generic/fixed fallbacks. It is correct as a fallback layer, but generic `eval()` should stay out of old texture/volume hot paths.
- `include/mitsuba/render/texture.h`: `SurfaceField` is a practical role adapter. Long term, reduce old Texture wording and duplicated fallback declarations.
- `src/render/texture.cpp`: keeps old texture behavior working. Hot fixed methods are still direct, which is good. Generic field methods allocate and validate; acceptable as adapter behavior.
- `include/mitsuba/render/volume.h`: `VolumeField` is the analogous role adapter. Same simplification comments as `SurfaceField`.
- `src/render/volume.cpp`: preserves old volume fixed paths and metadata. Generic adapter code is not hot-path friendly but is not used by migrated role consumers unless explicitly called.
- `include/mitsuba/render/fwd.h`: adds `Field`, `SurfaceField`, `VolumeField`, `FieldPtr`; keeps `Texture`/`Volume` aliases. This is useful for C++ migration but inconsistent with Python removal of `Texture`/`Volume`; decide whether C++ compatibility is required.
- `include/mitsuba/render/bsdf.h`: imports `Field` and fixes docs; necessary for attribute migration.
- `src/render/bsdf.cpp`: migrated attributes from `Texture` to `Field`; functionality is sound. Per-query metadata checks are the main efficiency issue.
- `include/mitsuba/render/film.h`, `src/render/film.cpp`: SRF uses `Field`; small necessary migration.
- `include/mitsuba/render/ior.h`: IOR table creation now goes through texture-role field creation. Necessary and centralized.
- `include/mitsuba/render/sensor.h`, `src/render/sensor.cpp`: SRF migrated to surface field and sample-spectrum validation added; correct.
- `include/mitsuba/render/shape.h`, `src/render/shape.cpp`: shape texture attributes now store fields and validate surface compatibility. Necessary; naming remains legacy (`texture_attribute`), which is acceptable for compatibility but not clean API.
- `include/mitsuba/render/sunsky.h`: imports `Field`; trivial migration.
- `src/render/srgb.cpp`: migrates D65/texture helper references to Field; trivial.
- `src/render/CMakeLists.txt`: adds `field.cpp`; necessary.

### Python Binding Layer

- `include/mitsuba/python/field.h`: only improves the `mitsuba::field<...>` caster comment; trivial and style-consistent.
- `include/mitsuba/python/docstr.h`: mostly doc grammar and texture/volume wording updates. Still stale because it does not document the new Field API comprehensively.
- `src/render/python/CMakeLists.txt`: replaces texture/volume bindings with field bindings; necessary.
- `src/render/python/field.cpp`: binds `FieldValueType`/`FieldDomain`; necessary and small.
- `src/render/python/field_v.cpp`: large binding/trampoline layer. It is functional and tested, but overloaded signatures plus Python-side wrappers are complex. Prefer keeping public overloads minimal before API freeze.
- `src/render/python/texture_v.cpp`: removed; correct if Python `Texture` is intentionally gone.
- `src/render/python/volume_v.cpp`: removed; correct if Python `Volume` is intentionally gone.
- `src/render/python/scene_v.cpp`: `register_texture` becomes `register_field`; necessary. This is a breaking Python API change and should be documented.
- `src/python/main.cpp`, `src/python/main_v.cpp`: exports Field and removes Texture/Volume variants from Python; necessary.
- `src/python/CMakeLists.txt`: adds `python.fields` stubs; necessary.
- `src/python/__init__.py`: auto-registers Python field plugins and wraps subclass methods. I fixed positional `active`; the wrapper remains a complexity hotspot.
- `src/python/python/fields/__init__.py`: registers `hashgridfield`, `permutofield`, `neuralfield` per variant. Necessary for Python-defined plugins.
- `src/python/python/fields/common.py`: shared metadata/encoding helpers. Good factoring; the `args is None` sentinel for Features means callers must know to use `eval_n()` or pass `args=[]`, which is an API wart.
- `src/python/python/fields/hashgrid.py`: thin registration wrapper; trivial.
- `src/python/python/fields/permuto.py`: thin registration wrapper; trivial.
- `src/python/python/fields/neural.py`: useful first neural field implementation. Output metadata validation is good. It supports fixed methods, but direct generic `eval()` has special spectrum-compatible behavior and should be documented clearly.
- `src/python/python/util.py`: adds `_SceneParameterFlags` to expose implicit differentiability in Python. Useful, but only tangentially related to Field and should not be bundled into a minimal refactor unless tests depend on it.
- `src/python_tests/test_util.py`: renames dummy texture plugins to fields and tests flag behavior. Mostly mechanical.

### Field Plugins And Adapters

- `src/fields/CMakeLists.txt`: consolidates texture and volume plugins under `fields`; necessary. Keeping both `grid` and `gridvolume` plugin names is a useful alias.
- `src/fields/bitmap.cpp` (renamed from `src/textures/bitmap.cpp`): migration is efficient. Fixed bitmap paths still use the original Dr.Jit texture implementation; Field metadata is added without disturbing hot eval.
- `src/fields/checkerboard.cpp` (renamed from `src/textures/checkerboard.cpp`): mostly mechanical migration to `SurfaceField`/`Field`; no new concern.
- `src/fields/mesh_attribute.cpp` (renamed from `src/textures/mesh_attribute.cpp`): mechanical migration; fine.
- `src/fields/gridvolume.cpp` (renamed from `src/volumes/grid.cpp`): preserves fixed volume hot paths and adds Field metadata/eval_n. Good. The `out_type()` mapping (`Array3` vs `Spectrum`) is explicit and compatible with role validation.
- `src/fields/constvolume.cpp`: new `VolumeField` replacement for old constant volume. Functional, but it probes `max()` and `mean()` in the constructor; use capability metadata instead.
- `src/fields/volume.cpp`: texture role adapter around a volume field. Necessary for legacy `<texture type="volume">`, but docs/default wording should be checked because the constructor now requires `volume`.
- `src/fields/sinusoidal.cpp`: simple feature field/encoding. Good addition; metadata is explicit and implementation is small.
- `src/textures/CMakeLists.txt`: deleted; correct after consolidation.
- `src/textures/volume.cpp`: replaced by `src/fields/volume.cpp`.
- `src/volumes/CMakeLists.txt`: deleted; correct after consolidation.
- `src/volumes/const.cpp`: replaced by `src/fields/constvolume.cpp`.

### BSDF Consumers

- `src/bsdfs/CMakeLists.txt`: adds `neuralbsdf`; necessary if keeping the plugin.
- `src/bsdfs/blendbsdf.cpp`: trivial migration from texture to surface field.
- `src/bsdfs/bumpmap.cpp`: important migration because it needs `eval_1_grad()`. Current direct-field rejection is correct, but it uses capability probing during construction.
- `src/bsdfs/circular.cpp`: trivial field migration.
- `src/bsdfs/conductor.cpp`: non-trivial migration of `specular_reflectance`, `eta`, `k` to fields plus spectral validation. Correct, but tied to `require_field_spectral_evaluable()`.
- `src/bsdfs/dielectric.cpp`: field migration for scalar/specular parameters; no independent issue found.
- `src/bsdfs/diffuse.cpp`: simple surface field migration plus spectral validation; correct.
- `src/bsdfs/hair.cpp`: mechanical field migration; no independent issue found.
- `src/bsdfs/mask.cpp`: trivial field migration.
- `src/bsdfs/neuralbsdf.cpp`: diffuse sample/eval/pdf contract is coherent. The implementation is better described as a diffuse BSDF whose reflectance is a field, not a general neural BSDF.
- `src/bsdfs/normalmap.cpp`: field migration for normal maps; no independent issue found.
- `src/bsdfs/plastic.cpp`: non-trivial because it needs `mean()` for energy compensation. Correct behavior, but construction-time mean probing should be replaced.
- `src/bsdfs/polarizer.cpp`: trivial field migration.
- `src/bsdfs/pplastic.cpp`: non-trivial rough/plastic field migration with mean/spectral requirements; same probing concern.
- `src/bsdfs/principled.cpp`: large but mostly mechanical field migration. Spectral validation is applied to `base_color`; scalar parameters still use scalar fixed evals and are acceptable.
- `src/bsdfs/principledthin.cpp`: same as `principled.cpp`; acceptable.
- `src/bsdfs/retarder.cpp`: field migration; no independent issue found.
- `src/bsdfs/roughconductor.cpp`: non-trivial spectral/IOR field migration; correct with the same validation-helper caveat.
- `src/bsdfs/roughdielectric.cpp`: field migration for texture parameters; no independent issue found.
- `src/bsdfs/roughplastic.cpp`: non-trivial mean/spectral validation; correct but uses probing helpers.
- `src/bsdfs/thindielectric.cpp`: field migration; no independent issue found.

### Emitters, Media, Phase, Film, Spectra

- `src/emitters/area.cpp`: emissive surface field migration with `sample_spectrum` validation; correct, but uses probe helper.
- `src/emitters/constant.cpp`: same as area emitter; also requires non-spatially varying radiance as before.
- `src/emitters/directional.cpp`: same pattern; correct.
- `src/emitters/directionalarea.cpp`: same pattern; correct.
- `src/emitters/envmap.cpp`: uses `Field::D65`; trivial and correct.
- `src/emitters/point.cpp`: same emissive field migration; correct.
- `src/emitters/projector.cpp`: uses `resolution_2d()` instead of texture `resolution()`; necessary for role abstraction.
- `src/emitters/spot.cpp`: migrates intensity and texture fields; spectral validation is correct but probe-based.
- `src/media/heterogeneous.cpp`: volume fields used for medium parameters; mechanical and necessary.
- `src/media/homogeneous.cpp`: accepts field-backed values and adds tests for inactive-sensitive behavior; correct.
- `src/phase/blendphase.cpp`: field migration for weight; trivial.
- `src/phase/sggx.cpp`: volume field migration for SGGX parameters; correct.
- `src/films/specfilm.cpp`: SRF field migration; necessary.
- `src/spectra/blackbody.cpp`, `src/spectra/irregular.cpp`, `src/spectra/rawconstant.cpp`, `src/spectra/regular.cpp`, `src/spectra/srgb.cpp`, `src/spectra/uniform.cpp`: mostly D65/Field role compatibility changes; trivial.
- `src/spectra/d65.cpp`: D65 now produces/uses fields; necessary for `Field::D65`.

### Tests

- `src/bsdfs/tests/test_bumpmap_field.py`: important direct-field negative/positive coverage for gradient requirements.
- `src/bsdfs/tests/test_neuralbsdf.py`: good coverage for diffuse contract and rejected invalid reflectance fields. CUDA-only portions should also run under LLVM when possible.
- `src/fields/tests/__init__.py`: package marker; trivial.
- `src/fields/tests/conftest.py`: useful backend fixture, but it picks CUDA first and therefore misses LLVM on machines with both.
- `src/fields/tests/test_ad.py`: good differentiability coverage for fields and adapters.
- `src/fields/tests/test_adapters.py`: good role adapter coverage, including Python fields.
- `src/fields/tests/test_field.py`: core Field/parser/Python API coverage. I added regressions for dedup, variant parser lookup, and positional active.
- `src/fields/tests/test_neural.py`: good neural/hashgrid/permuto coverage; make LLVM explicit.
- `src/fields/tests/test_storage.py`: good storage/role conversion coverage.
- `src/fields/tests/test_texture_bitmap.py` (renamed from texture test): pure migration; keep.
- `src/fields/tests/test_texture_mesh_attribute.py` (renamed): minor expected-name updates; keep.
- `src/fields/tests/test_texture_volume.py`: important mixed texture/volume role coverage.
- `src/fields/tests/test_texture_volume_adapter.py` (renamed): pure migration; keep.
- `src/fields/tests/test_volume_constant.py`: covers constant volume field behavior; good.
- `src/fields/tests/test_volume_grid.py` (renamed): important grid volume metadata/update coverage; CUDA-only metadata update should get LLVM equivalent if supported.
- `src/films/tests/test_specfilm.py`: fixes previously unasserted `dr.allclose()` checks and test naming; good.
- `src/media/tests/test_homogeneous.py`: useful field-backed medium value coverage.
- `src/phase/tests/test_sggx.py`: volume field coverage for SGGX; good.
- `src/render/tests/test_bsdf.py`: direct Field attribute coverage; good.
- `src/render/tests/test_mesh.py`: texture attribute behavior coverage; good.
- `src/render/tests/test_spectra.py`: D65/field spectrum coverage; good.
- `src/render/tests/test_texture.py`: verifies Python `Texture`/`Volume` API removal and FieldPtr behavior; good, but make sure the breaking change is documented.
- `src/volumes/tests/test_constant.py`: deleted because coverage moved to `src/fields/tests/test_volume_constant.py`.

## Suggested Follow-Up Order

1. Replace method-probing capability validation with metadata/capability bits.
2. Collapse `domain()` and query support booleans to one source of truth.
3. Finish Field API documentation and Python docstrings before API freeze.
4. Decide whether `neuralbsdf` should be renamed/scoped as diffuse-field reflectance, or expanded to pass direct field args.
5. Add explicit LLVM CI/test runs for Python neural fields and role adapters.
6. Run the benchmark suite against `mitsuba-std` again now that the standard volume baseline uses `gridvolume`.
