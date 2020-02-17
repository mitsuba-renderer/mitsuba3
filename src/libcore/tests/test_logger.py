import enoki as ek
import pytest
import mitsuba


def test01_custom(variant_scalar_rgb):
    from mitsuba.core import Thread, Appender, Formatter, Log, LogLevel

    # Install a custom formatter and appender and process a log message
    messages = []

    logger = Thread.thread().logger()
    formatter = logger.formatter()
    appenders = []
    while logger.appender_count() > 0:
        app = logger.appender(0)
        appenders.append(app)
        logger.remove_appender(app)

    try:
        class MyFormatter(Formatter):
            def format(self, level, theClass, thread, filename, line, msg):
                return "%i: class=%s, thread=%s, text=%s, filename=%s, ' \
                    'line=%i" % (level, str(theClass), thread.name(), msg,
                                 filename, line)

        class MyAppender(Appender):
            def append(self, level, text):
                messages.append(text)

        logger.set_formatter(MyFormatter())
        logger.add_appender(MyAppender())

        Log(LogLevel.Info, "This is a test message")
        assert len(messages) == 1
        assert messages[0].startswith(
                '200: class=None, thread=main, text=test01_custom(): This is a'
                ' test message, filename=')
    finally:
        logger.clear_appenders()
        for app in appenders:
            logger.add_appender(app)
        logger.set_formatter(formatter)
