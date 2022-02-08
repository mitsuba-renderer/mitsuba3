#pragma once

#include <mitsuba/core/timer.h>
#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief General-purpose progress reporter
 *
 * This class is used to track the progress of various operations that might
 * take longer than a second or so. It provides interactive feedback when
 * Mitsuba is run on the console, via the OpenGL GUI, or in Jupyter Notebook.
 */
class MI_EXPORT_LIB ProgressReporter : public Object {
public:
    /**
     * \brief Construct a new progress reporter.
     * \param label
     *     An identifying name for the operation taking place (e.g. "Rendering")
     * \param ptr
     *     Custom pointer payload to be delivered as part of progress messages
     */
    ProgressReporter(const std::string &label, void *payload = nullptr);

    /// Update the progress to \c progress (which should be in the range [0, 1])
    void update(float progress);

    MI_DECLARE_CLASS()
protected:
    ~ProgressReporter();

protected:
    Timer m_timer;
    std::string m_label;
    std::string m_line;
    size_t m_bar_start;
    size_t m_bar_size;
    size_t m_last_update;
    float m_last_progress;
    void *m_payload;
};

NAMESPACE_END(mitsuba)
