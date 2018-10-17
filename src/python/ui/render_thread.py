import mitsuba
from mitsuba.core import Thread

class RenderThread(Thread):
    """
    Simple class used to run Mitsuba renders asynchronously using the Python API.
    Designed to be used by the render's UI.
    """
    # TODO: check interplay of GIL / threading.Thread with Mitsuba performance.

    def __init__(self, integrator, thread_count = None,
                 progress_callback = (lambda: None),
                 done_callback = (lambda: None),
                 error_callback = (lambda: None),
                 args = [], kwargs = {}):
        super(RenderThread, self).__init__(name="RenderThread")
        self.integrator = integrator
        self.done = done_callback
        self.progress = progress_callback
        self.error = error_callback
        self.args = args
        self.kwargs = kwargs
        self.thread_count = thread_count

        self.set_priority(Thread.ELowPriority)

    def run(self):
        if self.thread_count is not None:
            mitsuba.set_thread_count(self.thread_count)
        cb_idx = self.integrator.register_callback(self.progress, 0)

        try:
            res = self.integrator.render(*self.args, **self.kwargs)
        except Exception as e:
            self.error(e)
            return
        finally:
            self.integrator.remove_callback(cb_idx)
            if self.thread_count is not None:
                mitsuba.set_thread_count(-1)
        self.done(res)
