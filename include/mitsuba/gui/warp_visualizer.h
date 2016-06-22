#pragma once

#include <mitsuba/core/platform.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/warp.h>

#include <Eigen/Core>
#include <nanogui/screen.h>
#include <nanogui/window.h>
#include <nanogui/glutil.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(warp)

/**
 * TODO: docstrings for this class
 */
class WarpVisualizationWidget : public nanogui::Screen {

public:
    WarpVisualizationWidget(int width, int height, std::string description);

    virtual ~WarpVisualizationWidget() {
        glDeleteTextures(2, &m_textures[0]);
    }

    bool runTest(double minExpFrequency, double significanceLevel);

    virtual void refresh();

    void setSamplingType(SamplingType s) { m_samplingType = s; }
    void setWarpType(WarpType w) { m_warpType = w; }
    void setParameterValue(float v) { m_parameterValue = v; }
    void setPointCount(int n) { m_pointCount = n; }

    bool isDrawingHistogram() { return m_drawHistogram; }
    void setDrawHistogram(bool draw) { m_drawHistogram = draw; }
    bool isDrawingGrid() { return m_drawGrid; }
    void setDrawGrid(bool draw) { m_drawGrid = draw; }

    nanogui::Window *window;

protected:
    virtual bool
    mouseMotionEvent(const nanogui::Vector2i &p, const nanogui::Vector2i & rel,
                     int button, int modifiers) override;

    virtual bool
    mouseButtonEvent(const nanogui::Vector2i &p, int button,
                     bool down, int modifiers) override;

    virtual bool
    keyboardEvent(int key, int scancode, int action, int modifiers) override;

    virtual void drawContents() override;

private:
    void drawHistogram(const nanogui::Vector2i &position,
                       const nanogui::Vector2i &dimensions, GLuint tex);

    void drawGrid(const Eigen::Matrix4f& mvp);


    void initializeVisualizerGUI();

    void framebufferSizeChanged() {
        m_arcball.setSize(mSize);
    }

private:
    std::unique_ptr<nanogui::GLShader> m_pointShader;
    std::unique_ptr<nanogui::GLShader> m_gridShader;
    std::unique_ptr<nanogui::GLShader> m_histogramShader;
    std::unique_ptr<nanogui::GLShader> m_arrowShader;
    GLuint m_textures[2];
    nanogui::Arcball m_arcball;

    bool m_drawHistogram, m_drawGrid;
    size_t m_pointCount, m_lineCount;
    SamplingType m_samplingType;
    WarpType m_warpType;
    float m_parameterValue;
    bool m_testResult;
    std::string m_testResultText;
};

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
