#include <mitsuba/core/progress.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/appender.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/string.h>

/// Escape strings to make them HTML-safe
std::string escape_html(const std::string& data) {
    std::string buffer;
    buffer.reserve(data.size());
    for (size_t pos = 0; pos != data.size(); ++pos) {
        switch(data[pos]) {
            case '&':  buffer.append("&amp;");       break;
            case '\"': buffer.append("&quot;");      break;
            case '\'': buffer.append("&apos;");      break;
            case '<':  buffer.append("&lt;");        break;
            case '>':  buffer.append("&gt;");        break;
            default:   buffer.append(&data[pos], 1); break;
        }
    }
    return buffer;
}

#if defined(__GNUG__)
#pragma GCC visibility push(hidden)
#endif

class JupyterNotebookAppender : public Appender {
public:
    JupyterNotebookAppender() {
        nb::module_ ipywidgets = nb::module_::import_("ipywidgets");
        m_float_progress = ipywidgets.attr("FloatProgress");
        m_html = ipywidgets.attr("HTML");
        m_layout = ipywidgets.attr("Layout");
        m_vbox = ipywidgets.attr("VBox");
        nb::module_ display = nb::module_::import_("IPython.display");
        m_flush = nb::module_::import_("sys").attr("stdout").attr("flush");
        m_display = display.attr("display");
        m_display_html = display.attr("display_html");
        m_label = m_bar = nb::none();
    }

    /// Append a line of text with the given log level
    virtual void append(mitsuba::LogLevel level, const std::string &text) override {
        std::string html_string;

        if (level == Info) {
            html_string = "<span style=\"font-family: monospace\">" + escape_html(text) + "</span>";
        } else {
            std::string col = "#000";
            if (level == Debug)
                col = "#bbb";
            else if (level == Warn || level == Error)
                col = "#f55";

            html_string = "<span style=\"font-family: monospace; color: " + col +
                          "\">" + escape_html(text) + "</span>";
        }

        nb::gil_scoped_acquire gil;
        m_display_html(html_string, "raw"_a = true);
        m_flush();
    }

    virtual void log_progress(float progress, const std::string &name,
        const std::string & /* formatted */, const std::string &eta,
        const void * /* ptr */) override {
        nb::gil_scoped_acquire gil;

        /* Heuristic: display the bar when it is created,
         * or when progress starts over.
         * Otherwise, the bar is only ever shown once. */
        make_and_display_progress_bar(progress == 0.f);

        m_bar.attr("value") = progress;
        m_label.attr("value") = escape_html(name) + " " + eta;
        if (progress == 1.f) {
            m_bar.attr("bar_style") = "success";
            m_label = m_bar = nb::none();
        }
        m_flush();
    }

    void make_and_display_progress_bar(bool display) {
        bool exists = !(m_label.is_none() || m_bar.is_none());
        if (!exists) {
            m_label = m_html();
            m_bar = m_float_progress(
                "layout"_a = m_layout("width"_a = "100%"), "bar_style"_a = "info",
                "min"_a = 0.0, "max"_a = 1.0);
        }

        if (!exists || display) {
            auto vbox = m_vbox("children"_a = nb::make_tuple(m_label, m_bar));
            m_display(vbox);
        }
    }
private:
    // Imports
    nb::object m_float_progress;
    nb::object m_html;
    nb::object m_layout;
    nb::object m_display;
    nb::object m_display_html;
    nb::object m_vbox;
    nb::object m_flush;

    // Progress bar
    nb::object m_bar;
    nb::object m_label;
};

#if defined(__GNUG__)
#pragma GCC visibility pop
#endif

MI_PY_EXPORT(ProgressReporter) {
    /* Install a custom appender for log + progress messages if Mitsuba is
     * running within Jupyter notebook */
    nb::dict modules = nb::module_::import_("sys").attr("modules");
    if (!modules.contains("ipykernel"))
        return;

    Logger *logger = Thread::thread()->logger();
    logger->clear_appenders();

    try {
        // First try to import ipywidgets
        nb::module_::import_("ipywidgets");
    } catch(const std::exception&) {
        nb::print("\033[93m[mitsuba] Warning: Couldn't import the ipywidgets "
                  "package. Installing this package is required for the system "
                  "to properly log messages and print in Jupyter notebooks!");
        return;
    }

    logger->add_appender(new JupyterNotebookAppender());
    (void) m;
}
