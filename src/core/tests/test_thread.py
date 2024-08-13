import pytest
import mitsuba as mi
from threading import Thread

def test01_use_scoped_thread_environment(variant_scalar_rgb):
    def use_scoped_set_thred_env(env):
        with mi.ScopedSetThreadEnvironment(environment):
            mi.Log(mi.LogLevel.Info, "Log from a thread environment.")

    environment = mi.ThreadEnvironment()
    thread = Thread(target = use_scoped_set_thred_env, args = (environment, ))
    thread.start()
    thread.join()
