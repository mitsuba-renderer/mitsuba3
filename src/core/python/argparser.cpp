#include <mitsuba/core/argparser.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <memory>

MI_PY_EXPORT(ArgParser) {
    nb::class_<ArgParser> argp(m, "ArgParser", D(ArgParser));
    nb::class_<ArgParser::Arg> argpa(argp, "Arg", D(ArgParser, Arg));

    argp.def(nb::init<>())
        .def("add", (const ArgParser::Arg * (ArgParser::*) (const std::string &, bool))
             &ArgParser::add, "prefix"_a, "extra"_a = false,
             nb::rv_policy::reference_internal,
             D(ArgParser, add, 2))
        .def("add", (const ArgParser::Arg * (ArgParser::*) (const std::vector<std::string> &, bool))
             &ArgParser::add, "prefixes"_a, "extra"_a = false,
             nb::rv_policy::reference_internal,
             D(ArgParser, add))
        .def("parse", [](ArgParser &a, std::vector<std::string> args) {
                std::unique_ptr<const char *[]> args_(new const char *[args.size()]);
                for (size_t i = 0; i<args.size(); ++i)
                args_[i] = args[i].c_str();
                a.parse((int) args.size(), args_.get());
            },
            D(ArgParser, parse))
        .def("executable_name", &ArgParser::executable_name);

    argpa.def("__bool__", &ArgParser::Arg::operator bool, D(ArgParser, Arg, operator, bool))
         .def("extra", &ArgParser::Arg::extra, D(ArgParser, Arg, extra))
         .def("count", &ArgParser::Arg::count, D(ArgParser, Arg, count))
         .def("next", &ArgParser::Arg::next, nb::rv_policy::reference_internal,
              D(ArgParser, Arg, next))
         .def("as_string", &ArgParser::Arg::as_string, D(ArgParser, Arg, as_string))
         .def("as_int", &ArgParser::Arg::as_int, D(ArgParser, Arg, as_int))
         .def("as_float", &ArgParser::Arg::as_float, D(ArgParser, Arg, as_float));
}

