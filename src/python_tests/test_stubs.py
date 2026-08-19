import re
import subprocess
import sys
from pathlib import Path

import mitsuba as mi


def test_field_stub_contract(tmp_path):
    root = Path(__file__).resolve().parents[2]
    output = tmp_path / "mitsuba.pyi"
    import_path = Path(mi.__file__).resolve().parents[1]

    subprocess.run(
        [
            sys.executable,
            str(root / "ext/nanobind/src/stubgen.py"),
            "-q",
            "-i",
            str(import_path),
            "-p",
            str(root / "src/python/stubs.pat"),
            "-m",
            "mitsuba",
            "-o",
            str(output),
        ],
        check=True,
    )

    stub = output.read_text(encoding="utf-8")
    field_value_type = re.search(
        r"^class FieldValueType\(enum\.Enum\):(?P<body>.*?)(?=^class )",
        stub,
        flags=re.MULTILINE | re.DOTALL,
    )
    assert field_value_type is not None
    assert re.search(
        r"^\s+Spectrum = 1$", field_value_type.group("body"), re.MULTILINE
    )
    assert "class ConditionalIrregular1DSpectrum" not in stub

    field = re.search(
        r"^class Field\(Object\):(?P<body>.*?)(?=^_FieldPtrCp:)",
        stub,
        flags=re.MULTILINE | re.DOTALL,
    )
    assert field is not None
    field_body = field.group("body")
    assert re.search(
        r"def eval\(self, si: SurfaceInteraction3f, active: [^)]+\) -> "
        r"(?!object\b)",
        field_body,
    )
    assert (
        "def resolution(self) -> Union[ScalarVector2i, ScalarVector3i]:"
        in field_body
    )
    assert "return the values as a Python list" in field_body
    assert "Pointer allocation and deallocation" not in field_body
    assert "When provided, ``args`` must contain" in field_body

    field_ptr = re.search(
        r"^class FieldPtr\(.*?\):(?P<body>.*?)(?=^class )",
        stub,
        flags=re.MULTILINE | re.DOTALL,
    )
    assert field_ptr is not None
    assert "When provided, ``args`` must contain" not in field_ptr.group("body")
