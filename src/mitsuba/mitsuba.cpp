#include <mitsuba/core/thread.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/xmlparser.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/argparser.h>

using namespace mitsuba;


int main(int argc, char *argv[]) {
    Class::staticInitialization();
    Thread::staticInitialization();
    Logger::staticInitialization();
    typedef std::vector<std::string> StringVec;

    ArgParser parser;
    auto arg_threads = parser.add(StringVec { "-t", "--threads" }, true);
    auto arg_extra = parser.add("", true);

    try {
        parser.parse(argc, argv);

        if (*arg_threads)
            std::cout << "Number of threads: "<< arg_threads->asInt() << std::endl;

        while (*arg_extra) {
            xml::load(arg_extra->asString());
            arg_extra = arg_extra->next();
        }
	} catch (const std::exception &e) {
		std::cerr << "Caught a critical exception: " << e.what() << std::endl;
		return -1;
	} catch (...) {
		std::cerr << "Caught a critical exception of unknown type!" << std::endl;
		return -1;
	}

    Logger::staticShutdown();
    Thread::staticShutdown();
    Class::staticShutdown();
    return 0;
}
