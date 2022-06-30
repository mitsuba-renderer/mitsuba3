#include <drjit/math.h>
#include <mitsuba/core/progress.h>
#include <mitsuba/core/logger.h>

NAMESPACE_BEGIN(mitsuba)

ProgressReporter::ProgressReporter(const std::string &label, void *payload)
    : m_label(label), m_line(util::terminal_width() + 1, ' '),
      m_bar_start(label.length() + 3), m_bar_size(0), m_payload(payload) {
    m_line[0] = '\r'; // Carriage return

    dr::ssize_t bar_size = (dr::ssize_t) m_line.length()
        - (dr::ssize_t) m_bar_start /* CR, Label, space, leading bracket */
        - 2 /* Trailing bracket and space */
        - 22 /* Max length for ETA string */;

    if (bar_size > 0) { /* Is there even space to draw a progress bar? */
        m_bar_size = bar_size;
        memcpy((char *) m_line.data() + 1, label.data(), label.length());
        m_line[m_bar_start - 1] = '[';
        m_line[m_bar_start + m_bar_size] = ']';
    }

    m_last_update = 0;
    m_last_progress = -1.f;
}

ProgressReporter::~ProgressReporter() { }

void ProgressReporter::update(float progress) {
    progress = dr::minimum(dr::maximum(progress, 0.f), 1.f);

    if (progress == m_last_progress)
        return;

    size_t elapsed = m_timer.value();
    if (progress != 1.f && (elapsed - m_last_update < 500 ||
                            dr::abs(progress - m_last_progress) < 0.01f))
        return; // Don't refresh too often

    float remaining = elapsed / progress * (1 - progress);
    std::string eta = "(" + util::time_string((float) elapsed)
        + ", ETA: " + util::time_string((float) remaining) + ")";
    if (eta.length() > 22)
        eta.resize(22);

    if (m_bar_size > 0) {
        size_t filled = dr::minimum(m_bar_size, (size_t) dr::round(m_bar_size * progress)),
               eta_pos = m_bar_start + m_bar_size + 2;
        memset((char *) m_line.data() + m_bar_start, '=', filled);
        memset((char *) m_line.data() + eta_pos, ' ', m_line.size() - eta_pos - 1);
        memcpy((char *) m_line.data() + eta_pos, eta.data(), eta.length());
    }

    Thread::thread()->logger()->log_progress(progress, m_label, m_line,
                                             eta, m_payload);
    m_last_update = elapsed;
}

MI_IMPLEMENT_CLASS(ProgressReporter, Object)

NAMESPACE_END(mitsuba)
