from threading import Thread

import pytest
import mitsuba as mi


def test01_add(variant_scalar_rgb):
    f = mi.AtomicFloat(5)
    threads = []

    def increment(f):
        for i in range(10):
            f += 1

    for j in range(0, 10):
        threads.append(Thread(target=increment, args=(f,)))
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert float(f) == 105
