#pragma once

#include <mitsuba/ui/fwd.h>
#include <nanogui/screen.h>

NAMESPACE_BEGIN(mitsuba)

namespace ng = nanogui;

/**
 * \brief Main class of the Mitsuba user interface
 */
class MI_EXPORT_UI MitsubaViewer : public ng::Screen {
public:
    struct Tab;

    /// Create a new viewer interface
    MitsubaViewer();

    /// Append an empty tab
    Tab *append_tab(const std::string &caption);

    /// Load content (a scene or an image) into a tab
    void load(Tab *tab, const fs::path &scene);

    using ng::Screen::perform_layout;
    virtual void perform_layout(NVGcontext* ctx) override;
    virtual bool keyboard_event(int key, int scancode, int action, int modifiers) override;

protected:
    void close_tab_impl(Tab *tab);

protected:
    ng::ref<ng::Button> m_btn_play, m_btn_stop, m_btn_reload;
    ng::ref<ng::PopupButton> m_btn_menu, m_btn_settings;
    ng::ref<ng::Widget> m_contents, m_progress_panel;
    ng::ref<ng::ProgressBar> m_progress_bar;
    ng::ref<ng::TabWidgetBase> m_tab_widget;
    ng::ref<ng::ImageView> m_view;
    std::vector<Tab *> m_tabs;
};

NAMESPACE_END(mitsuba)
