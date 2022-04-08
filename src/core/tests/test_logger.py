import pytest
import drjit as dr
import mitsuba as mi


def test01_custom(variant_scalar_rgb):
    # Install a custom formatter and appender and process a log message
    messages = []

    logger = mi.Thread.thread().logger()
    formatter = logger.formatter()
    appenders = []
    while logger.appender_count() > 0:
        app = logger.appender(0)
        appenders.append(app)
        logger.remove_appender(app)

    try:
        class MyFormatter(mi.Formatter):
            def format(self, level, theClass, thread, filename, line, msg):
                return "%i: class=%s, thread=%s, text=%s, filename=%s, ' \
                    'line=%i" % (level, str(theClass), thread.name(), msg,
                                 filename, line)

        class MyAppender(mi.Appender):
            def append(self, level, text):
                messages.append(text)

        logger.set_formatter(MyFormatter())
        logger.add_appender(MyAppender())

        mi.Log(mi.LogLevel.Warn, "This is a test message")
        assert len(messages) == 1
        assert messages[0].startswith(
                '300: class=None, thread=main, text=test01_custom(): This is a'
                ' test message, filename=')
    finally:
        logger.clear_appenders()
        for app in appenders:
            logger.add_appender(app)
        logger.set_formatter(formatter)
