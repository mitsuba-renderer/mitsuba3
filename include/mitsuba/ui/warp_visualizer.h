#pragma once

#include <mitsuba/core/platform.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/warp_adapters.h>

#include <Eigen/Core>
#include <nanogui/screen.h>
#include <nanogui/glutil.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(ui)

/// A NanoGUI widget to visualize 2D histograms (e.g. from Monte Carlo sampling techniques)
class MTS_EXPORT_UI HistogramWidget : public nanogui::Widget {

public:
    /// The parameters are passed to the \r nanogui::Screen constructor.
    WarpVisualizationWidget(int width, int height, std::string description);

    /// Destructor, releases the GL resources.
    virtual ~WarpVisualizationWidget() {
        glDeleteTextures(2, &m_textures[0]);
    }

    /** Run the Chi^2 test for the selected parameters, saves the results
     * and uploads the histograms (observed / expected) to the GPU
     * for rendering.
     */
    bool runTest(double minExpFrequency, double significanceLevel);

    /// Should be called after any UI interaction
    virtual void refresh();

    void setSamplingType(warp::SamplingType s) { m_samplingType = s; }
    void setWarpAdapter(std::shared_ptr<warp::WarpAdapter> wa) { m_warpAdapter = wa; }
    void setPointCount(int n) { m_pointCount = n; }

    bool isDrawingHistogram() { return m_drawHistogram; }
    void setDrawHistogram(bool draw) { m_drawHistogram = draw; }
    bool isDrawingGrid() { return m_drawGrid; }
    void setDrawGrid(bool draw) { m_drawGrid = draw; }

    /**
     * Fired upon Nanogui mouse motion event. Forwards the motion to the
     * underlying arcball to update the view.
     */
    virtual bool
    mouseMotionEvent(const nanogui::Vector2i &p, const nanogui::Vector2i & rel,
                     int button, int modifiers) override;

    /**
     * Fired upon Nanogui mouse button event. Forwards clicks to the
     * underlying arcball.
     */
    virtual bool
    mouseButtonEvent(const nanogui::Vector2i &p, int button,
                     bool down, int modifiers) override;

    /// Triggers a scene render, drawing the points, grid and histograms if enabled.
    virtual void drawContents() override;

private:
    /**
     * Draws the previously uploaded histogram texture \p tex at a given
     * position and dimensions on the canvas.
     */
    void drawHistogram(const nanogui::Vector2i &position,
                       const nanogui::Vector2i &dimensions, GLuint tex);

    /// Draws previously uploaded gridlines for a view matrix \p mvp on the canvas.
    void drawGrid(const Eigen::Matrix4f& mvp);

    /// Initializes the widget's shaders and performs a first draw.
    void initializeShaders();

    /// Updates the size of the underlying arcball, e.g. after a canvas resize.
    void framebufferSizeChanged() {
        // `mSize` is a member of `nanogui::Screen`
        m_arcball.setSize(mSize);
    }

private:
    std::unique_ptr<nanogui::GLShader> m_pointShader;
    std::unique_ptr<nanogui::GLShader> m_gridShader;
    std::unique_ptr<nanogui::GLShader> m_histogramShader;
    std::unique_ptr<nanogui::GLShader> m_arrowShader;
    GLuint m_textures[2];
    nanogui::Arcball m_arcball;

    warp::SamplingType m_samplingType;
    /// Holds the current warping method selected by the user. May be Identity.
    std::shared_ptr<warp::WarpAdapter> m_warpAdapter;

    bool m_drawHistogram, m_drawGrid;
    size_t m_pointCount, m_lineCount;
    bool m_testResult;
    std::string m_testResultText;
};

NAMESPACE_END(ui)
NAMESPACE_END(mitsuba)
