#pragma once

#include <mitsuba/core/util.h>
#include <chrono>

NAMESPACE_BEGIN(mitsuba)

class Timer {
public:
    using Unit = std::chrono::milliseconds;

    Timer() {
        start = std::chrono::system_clock::now();
    }

    size_t value() const {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<Unit>(now - start);
        return (size_t) duration.count();
    }

    size_t reset() {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<Unit>(now - start);
        start = now;
        return (size_t) duration.count();
    }

    void begin_stage(const std::string &name) {
        reset();
        std::cout << name << " .. ";
        std::cout.flush();
    }

    void end_stage(const std::string &str = "") {
        std::cout << "done. (took " << util::time_string((float) value());
        if (!str.empty())
            std::cout << ", " << str;
        std::cout << ")" << std::endl;
    }
private:
    std::chrono::system_clock::time_point start;
};

NAMESPACE_END(mitsuba)
