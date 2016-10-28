#include <mitsuba/core/argparser.h>
#include "python.h"

MTS_PY_EXPORT(ArgParser) {
    py::class_<ArgParser> argp(m, "ArgParser", DM(ArgParser));

    argp.def(py::init<>())
        .def("add", (const ArgParser::Arg * (ArgParser::*) (const std::string &, bool))
                    &ArgParser::add, py::arg("prefix"), py::arg("extra") = false,
                    py::return_value_policy::reference_internal,
                    DM(ArgParser, add, 2))
        .def("add", (const ArgParser::Arg * (ArgParser::*) (const std::vector<std::string> &, bool))
                    &ArgParser::add, py::arg("prefixes"), py::arg("extra") = false,
                    py::return_value_policy::reference_internal,
                    DM(ArgParser, add))
        .def("parse", [](ArgParser &a, std::vector<std::string> args) {
                std::unique_ptr<const char *[]> args_(new const char *[args.size()]);
                for (size_t i = 0; i<args.size(); ++i)
                   args_[i] = args[i].c_str();
                a.parse((int) args.size(), args_.get());
            },
            DM(ArgParser, parse))
        .def("executableName", &ArgParser::executableName);

    py::class_<ArgParser::Arg>(argp, "Arg", DM(ArgParser, Arg))
        .def("__bool__", &ArgParser::Arg::operator bool, DM(ArgParser, Arg, operator, bool))
        .def("extra", &ArgParser::Arg::extra, DM(ArgParser, Arg, extra))
        .def("count", &ArgParser::Arg::count, DM(ArgParser, Arg, count))
        .def("next", &ArgParser::Arg::next, py::return_value_policy::reference_internal,
                     DM(ArgParser, Arg, next))
        .def("asString", &ArgParser::Arg::asString, DM(ArgParser, Arg, asString))
        .def("asInt", &ArgParser::Arg::asInt, DM(ArgParser, Arg, asInt))
        .def("asFloat", &ArgParser::Arg::asFloat, DM(ArgParser, Arg, asFloat));
}

