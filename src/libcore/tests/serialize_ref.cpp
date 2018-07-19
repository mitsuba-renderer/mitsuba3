#include <iostream>
#include <mitsuba/core/astream.h>
#include <mitsuba/core/fstream.h>
#include <enoki/common.h>
#include <mitsuba/mitsuba.h>
#include <numeric>

using namespace mitsuba;

int main() {
    std::string fname = "reference.serialized";
    ref<FileStream> fstream =
        new FileStream(fname, FileStream::ETruncReadWrite);
    ref<AnnotatedStream> s = new AnnotatedStream(fstream.get(), true);

    s->set("top_char", (char) 'a');
    /* push */ {
        s->push("prefix1");
        s->set("prefix1_float", 1.f);
        s->set("prefix1_double", 1.);
        /* push */ {
            s->push("prefix2");
            s->set("prefix2_bool", true);
            s->pop();
        }
        s->set("prefix1_bool", false);
        s->set("prefix1_int16", (int16_t) 1);
        s->pop();
    }
    s->set("top_float_nan", std::numeric_limits<float>::quiet_NaN());
    s->set("top_double_nan", std::numeric_limits<double>::quiet_NaN());
    /* push */ {
        s->push("prefix3");
        s->set("prefix3_int8", (int8_t) 1);
        s->set("prefix3_uint8", (uint8_t) 1);
        s->set("prefix3_int16", (int16_t) 1);
        s->set("prefix3_uint16", (uint16_t) 1);
        s->set("prefix3_int32", (int32_t) 1);
        s->set("prefix3_uint32", (uint32_t) 1);
        s->set("prefix3_int64", (int64_t) 1);
        s->set("prefix3_uint64", (uint64_t) 1);
        s->pop();
    }
    /* push */ {
        // Empty prefix should not appear in the serialized file.
        s->push("prefix_empty");
        s->pop();
    }

    // Stream automatically closed & written on destruction.
    return 0;
}
