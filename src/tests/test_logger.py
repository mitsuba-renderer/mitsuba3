from mitsuba.core import Thread, Appender, Formatter, Log, EInfo


def test01_custom():
    # Install a custom formatter and appender and process a log message
    messages = []

    logger = Thread.thread().logger()
    formatter = logger.formatter()
    appenders = []
    while logger.appenderCount() > 0:
        app = logger.appender(0)
        appenders.append(app)
        logger.removeAppender(app)

    try:
        class MyFormatter(Formatter):
            def format(self, level, theClass, thread, filename, line, msg):
                return "%i: class=%s, thread=%s, text=%s, filename=%s, ' \
                    'line=%i" % (level, str(theClass), thread.name(), msg,
                                 filename, line)

        class MyAppender(Appender):
            def append(self, level, text):
                messages.append(text)

        logger.setFormatter(MyFormatter())
        logger.addAppender(MyAppender())

        Log(EInfo, "This is a test message")
        assert len(messages) == 1
        assert messages[0].startswith(
                '200: class=None, thread=main, text=test01_custom(): This is a'
                ' test message, filename=')
    finally:
        logger.clearAppenders()
        for app in appenders:
            logger.addAppender(app)
        logger.setFormatter(formatter)
