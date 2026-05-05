"""Standalone copy of the pbdr.benchmark utility used in this workspace.

The only modification is the `from . import log as _log` import — replaced
with a local minimal logger.
"""

import gc
import os
import time
from contextlib import contextmanager
from functools import wraps as _wraps
from typing import Callable

import _bench_log as _log  # local shim
import drjit as _dr


def wrap_function(
    label: str,
    dataframes: list = None,
    nb_runs: int = 4,
    nb_dry_runs: int = 0,
    log_level: int = 2,
    clear_cache: bool = True,
    no_async: bool = False,
):
    def wrapper(func: Callable):
        @_wraps(func)
        def f(*args, **kwargs):
            if "label" in kwargs.keys():
                suffix = f" [{kwargs['label']}]"
                del kwargs["label"]
            else:
                suffix = ""

            for i in range(nb_dry_runs):
                ret = func(*args, **kwargs)
                _dr.eval(ret)
                _dr.sync_thread()

            def single_run():
                clean_and_reset_drjit(clear_cache)
                start_time = time.time_ns() / 1e6
                ret = func(*args, **kwargs)
                _dr.eval(ret)
                _dr.sync_thread()
                total_time = float(time.time_ns() / 1e6 - start_time)
                return ret, total_time

            if log_level > 0:
                _log.info(f'Benchmarking: "{label}{suffix}" ...')

            total_runs = nb_runs if no_async else 2 * nb_runs

            with _dr.scoped_set_flag(_dr.JitFlag.KernelHistory, True):
                with _log.ProgressBar(
                    "Running", total_runs, disabled=(log_level < 1)
                ) as progress:
                    with _dr.scoped_set_flag(_dr.JitFlag.LaunchBlocking, True):
                        codegen_times = []
                        backend_times = []
                        execution_times = []
                        jitting_times = []
                        sync_total_times = []

                        for i in range(nb_runs):
                            ret, sync_total_time = single_run()
                            history = _dr.kernel_history([_dr.KernelType.JIT])
                            codegen_time = sum([k["codegen_time"] for k in history])
                            backend_time = sum([k["backend_time"] for k in history])
                            execution_time = sum([k["execution_time"] for k in history])
                            jitting_time = sync_total_time - (
                                codegen_time + backend_time + execution_time
                            )

                            codegen_times.append(codegen_time)
                            backend_times.append(backend_time)
                            execution_times.append(execution_time)
                            jitting_times.append(jitting_time)
                            sync_total_times.append(sync_total_time)

                            progress.step()

                    if not no_async:
                        with _dr.scoped_set_flag(_dr.JitFlag.LaunchBlocking, False):
                            async_total_times = []
                            for i in range(nb_runs):
                                ret, total_time_async = single_run()
                                async_total_times.append(total_time_async)
                                progress.step()

                def mean(x):
                    return sum(x) / float(nb_runs)

                def std(x):
                    return _dr.sqrt(mean([v**2 for v in x]) - mean(x) ** 2)

                if not no_async:
                    async_total_time = mean(async_total_times)
                sync_total_time = mean(sync_total_times)
                jitting_time = mean(jitting_times)
                codegen_time = mean(codegen_times)
                backend_time = mean(backend_times)
                execution_time = mean(execution_times)

                if not no_async:
                    async_total_time_std = std(async_total_times)
                sync_total_time_std = std(sync_total_times)
                jitting_time_std = std(jitting_times)
                codegen_time_std = std(codegen_times)
                backend_time_std = std(backend_times)
                execution_time_std = std(execution_times)

                if log_level > 1:
                    _log.info(f"Results (averaged over {nb_runs} runs):")
                    if not no_async:
                        _log.info(
                            f"    - Total time (async): {async_total_time:.2f} ms (± {async_total_time_std:.2f})"
                        )
                        _log.info(
                            f"    - Total time (sync):  {sync_total_time:.2f} ms (± {sync_total_time_std:.2f}) -> (async perf. gain: {sync_total_time - async_total_time:.2f} ms)"
                        )
                    else:
                        _log.info(
                            f"    - Total time:         {sync_total_time:.2f} ms (± {sync_total_time_std:.2f})"
                        )
                    _log.info(
                        f"    - Jitting time:       {jitting_time:.2f} ms (± {jitting_time_std:.2f})"
                    )
                    _log.info(
                        f"    - Codegen time:       {codegen_time:.2f} ms (± {codegen_time_std:.2f})"
                    )
                    _log.info(
                        f"    - Backend time:       {backend_time:.2f} ms (± {backend_time_std:.2f})"
                    )
                    _log.info(
                        f"    - Execution time:     {execution_time:.2f} ms (± {execution_time_std:.2f})"
                    )

                if dataframes is not None:
                    df = {
                        "label": label,
                        "suffix": suffix,
                        "nb_runs": nb_runs,
                        "sync_total_time": sync_total_time,
                        "codegen_time": codegen_time,
                        "backend_time": backend_time,
                        "execution_time": execution_time,
                        "jitting_time": jitting_time,
                        "sync_total_time_std": sync_total_time_std,
                        "codegen_time_std": codegen_time_std,
                        "backend_time_std": backend_time_std,
                        "execution_time_std": execution_time_std,
                        "jitting_time_std": jitting_time_std,
                    }
                    if not no_async:
                        df["async_total_time"] = async_total_time
                        df["async_total_time_std"] = async_total_time_std
                    dataframes.append(df)

            return ret

        return f

    return wrapper


def clear_cache_folders(clear_drjit=True, clear_nvdia=True):
    import shutil
    import time as _t

    if os.name != "nt":

        def clear(path):
            if os.path.exists(path):
                _t.sleep(0.1)
                shutil.rmtree(path, ignore_errors=True)
                while os.path.exists(path):
                    _t.sleep(0.1)
                os.mkdir(path)
                _t.sleep(0.1)

        if clear_drjit:
            clear(os.path.join(os.path.expanduser("~"), ".drjit/"))
        if clear_nvdia:
            clear(os.path.join(os.path.expanduser("~"), ".nv/"))


def clean_and_reset_drjit(clear_cache=True):
    _dr.kernel_history_clear()
    gc.collect()
    gc.collect()
    _dr.flush_malloc_cache()
    _dr.detail.malloc_clear_statistics()
    if clear_cache:
        clear_cache_folders()
        _dr.flush_kernel_cache()
