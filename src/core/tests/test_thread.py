import pytest
import mitsuba as mi
from threading import Thread


def test01_use_scoped_thread_environment(variant_scalar_rgb):
    def use_scoped_set_thread_env(env):
        with mi.ScopedSetThreadEnvironment(environment):
            mi.Log(mi.LogLevel.Info, 'Log from a thread environment.')

    environment = mi.ThreadEnvironment()
    thread = Thread(target=use_scoped_set_thread_env, args=(environment, ))
    thread.start()
    thread.join()


def test02_log_from_new_thread(variant_scalar_rgb, tmp_path):

    # We use a StreamAppender to capture the output.
    log_path = tmp_path / 'log.txt'
    appender = mi.StreamAppender(str(log_path))

    log_str = 'Log from a thread environment.'

    def print_to_log():
        logger = mi.Thread.thread().logger()
        assert logger is not None
        logger.add_appender(appender)
        mi.set_log_level(mi.LogLevel.Info)
        mi.Log(mi.LogLevel.Info, log_str)

    thread = Thread(target=print_to_log)
    thread.start()
    thread.join()

    log = appender.read_log()
    assert 'print_to_log' in log
    assert log_str in log


def test03_access_file_resolver_from_new_thread(variant_scalar_rgb):
    def access_fresolver():
        fs = mi.Thread.thread().file_resolver()
        assert fs is not None
        fs.resolve('./some_path')
        n_paths = len(fs)
        fs.prepend('./some_folder')
        fs.prepend('./some_folder2')
        assert len(fs) == n_paths + 2

    fs = mi.Thread.thread().file_resolver()
    n_paths = len(fs)

    thread = Thread(target=access_fresolver)
    thread.start()
    thread.join()

    # The original file resolver remains unchanged.
    assert len(fs) == n_paths
