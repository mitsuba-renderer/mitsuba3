try:
    import unittest2 as unittest
except:
    import unittest

from mitsuba import Thread, Logger, Appender, Formatter, Log, EInfo


class LoggerTest(unittest.TestCase):
    def setUp(self):
        self.logger = Thread.getThread().getLogger()
        self.formatter = self.logger.getFormatter()
        self.appenders = []
        while self.logger.getAppenderCount() > 0:
            app = self.logger.getAppender(0)
            self.appenders.append(app)
            self.logger.removeAppender(app)

    def tearDown(self):
        self.logger.clearAppenders()
        for app in self.appenders:
            self.logger.addAppender(app)
        self.logger.setFormatter(self.formatter)

    def test01_custom(self):
        messages = []

        # Install a custom formatter and appender and process a log message
        class MyFormatter(Formatter):
            def format(self, level, theClass, thread, filename, line, msg):
                return "%i: class=%s, thread=%s, text=%s, filename=%s, ' \
                    'line=%i" % (level, str(theClass), thread.getName(), msg,
                                 filename, line)

        class MyAppender(Appender):
            def append(self, level, text):
                messages.append(text)

        self.logger.setFormatter(MyFormatter())
        self.logger.addAppender(MyAppender())

        Log(EInfo, "This is a test message")
        self.assertEqual(len(messages), 1)
        self.assertTrue(messages[0].startswith(
                '200: class=None, thread=main, text=test01_custom(): This is a'
                ' test message, filename='))


if __name__ == '__main__':
    unittest.main()
