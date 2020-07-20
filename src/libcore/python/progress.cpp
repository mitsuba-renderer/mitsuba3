#include <mitsuba/core/progress.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/appender.h>
#include <mitsuba/core/thread.h>
#include <pybind11/eval.h>
#include <mitsuba/python/python.h>

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

class JupyterNotebookAppender : public Appender {
public:
    JupyterNotebookAppender() {
        py::module ipywidgets = py::module::import("ipywidgets");
        m_float_progress = ipywidgets.attr("FloatProgress");
        m_html = ipywidgets.attr("HTML");
        m_layout = ipywidgets.attr("Layout");
        m_vbox = ipywidgets.attr("VBox");
        py::module display = py::module::import("IPython.display");
        m_flush = py::module::import("sys").attr("stdout").attr("flush");
        m_display = display.attr("display");
        m_display_html = display.attr("display_html");
        m_label = m_bar = py::none();
    }

    /// Append a line of text with the given log level
    virtual void append(LogLevel level, const std::string &text) override {
        std::string col = "#000";
        if (level == Debug)
            col = "#bbb";
        else if (level == Warn || level == Error)
            col = "#f55";
        auto html_string =
             "<span style=\"font-family: monospace; color: " + col + "\">" + escape_html(text) + "</span>" ;
        py::gil_scoped_acquire gil;
        m_display_html(html_string, "raw"_a = true);
        m_flush();
    }

    virtual void log_progress(float progress, const std::string &name,
        const std::string & /* formatted */, const std::string &eta,
        const void * /* ptr */) override {
        py::gil_scoped_acquire gil;
        if (m_label.is_none() || m_bar.is_none())
            make_progress_bar();
        m_bar.attr("value") = progress;
        m_label.attr("value") = escape_html(name) + " " + eta;
        if (progress == 1.f) {
            m_bar.attr("bar_style") = "success";
            m_label = m_bar = py::none();
        }
        m_flush();
    }
    void make_progress_bar() {
        m_bar = m_float_progress(
            "layout"_a = m_layout("width"_a = "100%"), "bar_style"_a = "info",
            "min"_a = 0.0, "max"_a = 1.0);
        m_label = m_html();
        auto vbox = m_vbox("children"_a = py::make_tuple(m_label, m_bar));
        m_display(vbox);
    }
private:
    // Imports
    py::object m_float_progress;
    py::object m_html;
    py::object m_layout;
    py::object m_display;
    py::object m_display_html;
    py::object m_vbox;
    py::object m_flush;

    // Progress bar
    py::object m_bar;
    py::object m_label;
};

MTS_PY_EXPORT(ProgressReporter) {
    /* Install a custom appender for log + progress messages if Mitsuba is
     * running within Jupyter notebook */
    py::object modules = py::module::import("sys").attr("modules");
    if (!modules.contains("ipykernel"))
        return;

    Logger *logger = Thread::thread()->logger();
    logger->clear_appenders();
    logger->add_appender(new JupyterNotebookAppender());
    (void) m;
}
