import importlib.util
import io
import re
from pathlib import Path

import pytest


pytest.importorskip("clang")
module_path = Path(__file__).resolve().parents[2] / "resources" / "mkdocs.py"
spec = importlib.util.spec_from_file_location("mitsuba_mkdocs", module_path)
mkdocs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mkdocs)


def test_overload_suffix_does_not_collide_with_suffixed_method():
    comments = [
        ('__doc_mitsuba_Field_eval', 'field.h', 'generic 1'),
        ('__doc_mitsuba_Field_eval', 'field.h', 'generic 2'),
        ('__doc_mitsuba_Field_eval', 'field.h', 'generic 3'),
        ('__doc_mitsuba_Field_eval', 'field.h', 'generic 4'),
        ('__doc_mitsuba_Field_eval_3', 'field.h', 'specialized 1'),
        ('__doc_mitsuba_Field_eval_3', 'field.h', 'specialized 2'),
    ]

    output = io.StringIO()
    mkdocs.write_header(comments, output)
    header = output.getvalue()
    declarations = re.findall(
        r'^static const char \*([A-Za-z0-9_]+)\s*=', header, re.MULTILINE)

    assert declarations == [
        '__doc_mitsuba_Field_eval',
        '__doc_mitsuba_Field_eval_2',
        '__doc_mitsuba_Field_eval_3',
        '__doc_mitsuba_Field_eval_4',
        '__doc_mitsuba_Field_eval_3_2',
        '__doc_mitsuba_Field_eval_3_3',
    ]
    assert len(declarations) == len(set(declarations))

    output_again = io.StringIO()
    mkdocs.write_header(comments, output_again)
    assert output_again.getvalue() == header
