try:
    import unittest2 as unittest
except:
    import unittest

from mitsuba import AtomicFloat
from threading import Thread


class AtomicTest(unittest.TestCase):
    def test01_add(self):
        f = AtomicFloat(5)
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
        self.assertEqual(float(f), 105)

if __name__ == '__main__':
    unittest.main()
