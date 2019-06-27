#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/jit.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/render/common.h>

#include <nanogui/nanogui.h>
#include <nanogui/common.h>
#include "mtsgui_resources.h"

using namespace mitsuba;

NAMESPACE_BEGIN(nanogui)


NAMESPACE_END(nanogui)

class MitsubaApp : public nanogui::Screen {
public:
    MitsubaApp(): nanogui::Screen(Vector2i(1024, 768), "Mitsuba 2") {
        using namespace nanogui;
        using nanogui::Vector2i;

        m_contents = new Widget(this);

        AdvancedGridLayout *layout = new AdvancedGridLayout({30, 0}, {50, 5, 0}, 5);
        layout->set_row_stretch(0, 1);
        layout->set_col_stretch(1, 1);

        m_contents->set_layout(layout);
        m_contents->set_size(m_size);

        Widget *tools = new Widget(m_contents);
        tools->set_layout(new BoxLayout(Orientation::Vertical,
                                        Alignment::Middle, 0, 6));

        m_btn_menu = new PopupButton(tools, "", ENTYPO_ICON_MENU);
        Popup *menu = m_btn_menu->popup();

        menu->set_layout(new GroupLayout());
        menu->set_visible(true);
        menu->set_size(Vector2i(200, 140));
        new Button(menu, "Open ..", ENTYPO_ICON_FOLDER);
        PopupButton *recent = new PopupButton(menu, "Open Recent");
        auto recent_popup = recent->popup();
        recent_popup->set_layout(new GroupLayout());
        new Button(recent_popup, "scene1.xml");
        new Button(recent_popup, "scene2.xml");
        new Button(recent_popup, "scene3.xml");
        new Button(menu, "Export image ..", ENTYPO_ICON_EXPORT);

        m_btn_play = new Button(tools, "", ENTYPO_ICON_CONTROLLER_PLAY);
        m_btn_play->set_text_color(nanogui::Color(100, 255, 100, 150));
        m_btn_play->set_callback([this]() {
                m_btn_play->set_icon(ENTYPO_ICON_CONTROLLER_PAUS);
                m_btn_play->set_text_color(nanogui::Color(255, 255, 255, 150));
                m_btn_stop->set_enabled(true);
            }
        );
        m_btn_play->set_tooltip("Render");

        m_btn_stop = new Button(tools, "", ENTYPO_ICON_CONTROLLER_STOP);
        m_btn_stop->set_text_color(nanogui::Color(255, 100, 100, 150));
        m_btn_stop->set_enabled(false);
        m_btn_stop->set_tooltip("Stop rendering");

        m_btn_reload = new Button(tools, "", ENTYPO_ICON_CYCLE);
        m_btn_reload->set_tooltip("Reload file");

        m_btn_settings = new PopupButton(tools, "", ENTYPO_ICON_COG);
        m_btn_settings->set_tooltip("Scene configuration");

        auto settings_popup = m_btn_settings->popup();
        AdvancedGridLayout *settings_layout = new AdvancedGridLayout({30, 0, 15, 50}, {0, 5, 0, 5, 0, 5, 0}, 5);
        settings_popup->set_layout(settings_layout);
        settings_layout->set_col_stretch(0, 1);
        settings_layout->set_col_stretch(1, 1);
        settings_layout->set_col_stretch(2, 1);
        settings_layout->set_col_stretch(3, 10);
        settings_layout->set_row_stretch(1, 1);
        settings_layout->set_row_stretch(5, 1);

        using Anchor = nanogui::AdvancedGridLayout::Anchor;
        settings_layout->set_anchor(new Label(settings_popup, "Integrator", "sans-bold"), Anchor(0, 0, 4, 1));
        settings_layout->set_anchor(new Label(settings_popup, "Max depth"), Anchor(1, 1));
        settings_layout->set_anchor(new Label(settings_popup, "Sampler", "sans-bold"), Anchor(0, 4, 4, 1));
        settings_layout->set_anchor(new Label(settings_popup, "Sample count"), Anchor(1, 5));
        IntBox<uint32_t> *ib1 = new IntBox<uint32_t>(settings_popup);
        IntBox<uint32_t> *ib2 = new IntBox<uint32_t>(settings_popup);
        ib1->set_editable(true);
        ib2->set_editable(true);
        ib1->set_alignment(TextBox::Alignment::Right);
        ib2->set_alignment(TextBox::Alignment::Right);
        ib1->set_fixed_height(25);
        ib2->set_fixed_height(25);
        settings_layout->set_anchor(ib1, Anchor(3, 1));
        settings_layout->set_anchor(ib2, Anchor(3, 5));
        settings_popup->set_size(Vector2i(0, 0));
        settings_popup->set_size(settings_popup->preferred_size(nvg_context()));

        for (Button *b : { (Button *) m_btn_menu, m_btn_play, m_btn_stop, m_btn_reload,
                           (Button *) m_btn_settings }) {
            b->set_fixed_size(Vector2i(25, 25));
            PopupButton *pb = dynamic_cast<PopupButton *>(b);
            if (pb) {
                pb->set_chevron_icon(0);
                pb->popup()->set_anchor_offset(12);
                pb->popup()->set_anchor_size(12);
            }
        }

        TabWidgetBase *tab = new TabWidgetBase(m_contents);
        tab->append_tab("cbox.xml");
        tab->append_tab("matpreview.xml");
        tab->append_tab("hydra.xml");
        tab->set_tabs_closeable(true);

        tab->set_popup_callback([tab](int id, Screen *screen) -> Popup * {
            Popup *popup = new Popup(screen);
            Button *b1 = new Button(popup, "Close", ENTYPO_ICON_CIRCLE_WITH_CROSS);
            Button *b2 = new Button(popup, "Duplicate", ENTYPO_ICON_CIRCLE_WITH_PLUS);
            b1->set_callback([tab, id]() { tab->remove_tab(id); });
            b2->set_callback([tab, id]() { tab->insert_tab(tab->tab_index(id), tab->tab_caption(id)); });
            return popup;
        });

        layout->set_anchor(tools, Anchor(0, 0, Alignment::Minimum, Alignment::Minimum));
        layout->set_anchor(tab, Anchor(1, 0, Alignment::Fill, Alignment::Fill));

        m_progress_panel = new Widget(m_contents);
        layout->set_anchor(m_progress_panel, Anchor(1, 2, Alignment::Fill, Alignment::Fill));

        Label *label1 = new Label(m_progress_panel, "Rendering:", "sans-bold");
        Label *label2 = new Label(m_progress_panel, "30% (ETA: 0.2s)");
        m_progress_bar = new ProgressBar(m_progress_panel);
        m_progress_bar->set_value(.3f);

        AdvancedGridLayout *progress_layout = new AdvancedGridLayout({0, 5, 0, 10, 0}, {0}, 0);
        progress_layout->set_col_stretch(4, 1);
        m_progress_panel->set_layout(progress_layout);
        progress_layout->set_anchor(label1, Anchor(0, 0));
        progress_layout->set_anchor(label2, Anchor(2, 0));
        progress_layout->set_anchor(m_progress_bar, Anchor(4, 0));

        perform_layout();
    }

    void draw(NVGcontext* ctx) override {
        Screen::draw(ctx);
        int logo = nvgImageIcon(ctx, mitsuba_logo);

        int iw, ih;
        nvgImageSize(ctx, logo, &iw, &ih);
        iw /= 2;
        ih /= 2;
        NVGpaint img_paint = nvgImagePattern(ctx, (m_size.x() - iw) / 2,
                                                  (m_size.y() - ih) / 2,
                                             iw, ih, 0, logo, 1.f);
        nvgBeginPath(ctx);
        nvgRect(ctx, 0, 0, m_size.x(), m_size.y());

        nvgFillPaint(ctx, img_paint);
        nvgFill(ctx);
    }

    bool resize_event(const nanogui::Vector2i& size) override {
        if (Screen::resize_event(size))
            return true;
        glfwGetWindowSize(m_glfw_window, &m_size[0], &m_size[1]);
        m_progress_panel->set_size(Vector2i(0, 0));
        m_contents->set_size(m_size);
        perform_layout();
        //m_redraw = true;
        //draw_all();
        return false;
    }

    bool keyboard_event(int key, int scancode, int action, int modifiers) override {
        if (Screen::keyboard_event(key, scancode, action, modifiers))
            return true;
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            set_visible(false);
            return true;
        }
        return false;
    }

private:
    nanogui::Button *m_btn_play, *m_btn_stop, *m_btn_reload;
    nanogui::PopupButton *m_btn_menu, *m_btn_settings;
    nanogui::Widget *m_contents, *m_progress_panel;
    nanogui::ProgressBar *m_progress_bar;
};

int main(int argc, char *argv[]) {
    Jit::static_initialization();
    Class::static_initialization();
    Thread::static_initialization();
    Logger::static_initialization();
    Bitmap::static_initialization();
    Profiler::static_initialization();

    /* Ensure that the mitsuba-render shared library is loaded */
    librender_nop();

    nanogui::init();

    {
        nanogui::ref<MitsubaApp> app = new MitsubaApp();
        app->draw_all();
        app->set_visible(true);
        nanogui::mainloop(-1);
    }
    nanogui::shutdown();

    Profiler::static_shutdown();
    Bitmap::static_shutdown();
    Logger::static_shutdown();
    Thread::static_shutdown();
    Class::static_shutdown();
    Jit::static_shutdown();

    return 0;
}
