#pragma once

#include <mitsuba/ui/fwd.h>
#include <nanogui/screen.h>

NAMESPACE_BEGIN(mitsuba)

namespace ng = nanogui;

class MTS_EXPORT_UI MitsubaViewer : public nanogui::Screen {
public:
    MitsubaViewer();

    virtual bool keyboard_event(int key, int scancode, int action, int modifiers) override;

protected:
    ng::ref<ng::Button> m_btn_play, m_btn_stop, m_btn_reload;
    ng::ref<ng::PopupButton> m_btn_menu, m_btn_settings;
    ng::ref<ng::Widget> m_contents, m_progress_panel;
    ng::ref<ng::ProgressBar> m_progress_bar;
    ng::ref<ng::ImageView> m_view;
};

NAMESPACE_END(mitsuba)
