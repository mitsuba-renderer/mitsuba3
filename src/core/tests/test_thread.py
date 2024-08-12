import pytest
import mitsuba as mi

def test01_use_scoped_thread_environment(variant_scalar_rgb):
    environment = mi.ThreadEnvironment()
    with mi.ScopedSetThreadEnvironment(environment):
        mi.Log(mi.LogLevel.Info, "Log from a thread environment.")
