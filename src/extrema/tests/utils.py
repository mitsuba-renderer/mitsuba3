"""Helpers shared by the extremum structure tests."""

import drjit as dr
import pytest

MAX_STEPS = 512


def walk_iter(extremum, ray, mint, maxt):
    state = extremum.begin(ray, mint, maxt)
    out = []
    for _ in range(MAX_STEPS):
        if not state.mint < state.maxt:
            return out
        segment, state = extremum.next(state, ray)
        out.append((segment.mint, segment.maxt, segment.minorant(), segment.majorant()))
    pytest.fail("traversal did not terminate")


def sample_iter(structure, ray, mint, maxt, target_ot):
    """Delta-tracking oracle over the structure's piecewise-constant majorant:
    returns ``(distance, leftover_ot)``, ``distance`` infinite if
    ``target_ot`` is not reached before ``maxt``."""
    for lo, hi, _, majorant in walk_iter(structure, ray, mint, maxt):
        lo, hi = max(lo, mint), min(hi, maxt)
        if hi <= lo:
            continue
        ot = (hi - lo) * majorant
        if target_ot < ot:
            return lo + target_ot / max(majorant, 1e-7), target_ot
        target_ot -= ot
    return dr.inf, target_ot

