#include <mitsuba/gui/warp_visualizer.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/warp_adapters.h>

#include <pcg32.h>

namespace {
/// HSLS code (shaders)
const std::string kPointVertexShader =
    "#version 330\n"
    "uniform mat4 mvp;\n"
    "in vec3 position;\n"
    "in vec3 color;\n"
    "out vec3 frag_color;\n"
    "void main() {\n"
    "    gl_Position = mvp * vec4(position, 1.0);\n"
    "    if (isnan(position.r)) /* nan (missing value) */\n"
    "        frag_color = vec3(0.0);\n"
    "    else\n"
    "        frag_color = color;\n"
    "}";
const std::string kPointFragmentShader =
    "#version 330\n"
    "in vec3 frag_color;\n"
    "out vec4 out_color;\n"
    "void main() {\n"
    "    if (frag_color == vec3(0.0))\n"
    "        discard;\n"
    "    out_color = vec4(frag_color, 1.0);\n"
    "}";

const std::string kGridVertexShader =
    "#version 330\n"
    "uniform mat4 mvp;\n"
    "in vec3 position;\n"
    "void main() {\n"
    "    gl_Position = mvp * vec4(position, 1.0);\n"
    "}";
const std::string kGridFragmentShader =
    "#version 330\n"
    "out vec4 out_color;\n"
    "void main() {\n"
    "    out_color = vec4(vec3(1.0), 0.4);\n"
    "}";

const std::string kArrowVertexShader =
    "#version 330\n"
    "uniform mat4 mvp;\n"
    "in vec3 position;\n"
    "void main() {\n"
    "    gl_Position = mvp * vec4(position, 1.0);\n"
    "}";
const std::string kArrowFragmentShader =
    "#version 330\n"
    "out vec4 out_color;\n"
    "void main() {\n"
    "    out_color = vec4(vec3(1.0), 0.4);\n"
    "}";

const std::string kHistogramVertexShader =
    "#version 330\n"
    "uniform mat4 mvp;\n"
    "in vec2 position;\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "    gl_Position = mvp * vec4(position, 0.0, 1.0);\n"
    "    uv = position;\n"
    "}";
const std::string kHistogramFragmentShader =
    "#version 330\n"
    "out vec4 out_color;\n"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "/* http://paulbourke.net/texture_colour/colourspace/ */\n"
    "vec3 colormap(float v, float vmin, float vmax) {\n"
    "    vec3 c = vec3(1.0);\n"
    "    if (v < vmin)\n"
    "        v = vmin;\n"
    "    if (v > vmax)\n"
    "        v = vmax;\n"
    "    float dv = vmax - vmin;\n"
    "    \n"
    "    if (v < (vmin + 0.25 * dv)) {\n"
    "        c.r = 0.0;\n"
    "        c.g = 4.0 * (v - vmin) / dv;\n"
    "    } else if (v < (vmin + 0.5 * dv)) {\n"
    "        c.r = 0.0;\n"
    "        c.b = 1.0 + 4.0 * (vmin + 0.25 * dv - v) / dv;\n"
    "    } else if (v < (vmin + 0.75 * dv)) {\n"
    "        c.r = 4.0 * (v - vmin - 0.5 * dv) / dv;\n"
    "        c.b = 0.0;\n"
    "    } else {\n"
    "        c.g = 1.0 + 4.0 * (vmin + 0.75 * dv - v) / dv;\n"
    "        c.b = 0.0;\n"
    "    }\n"
    "    return c;\n"
    "}\n"
    "void main() {\n"
    "    float value = texture(tex, uv).r;\n"
    "    out_color = vec4(colormap(value, 0.0, 1.0), 1.0);\n"
    "}";
}  // end anonymous namespace

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(warp)

using nanogui::Arcball;
using nanogui::Color;
using nanogui::GLShader;
using nanogui::ortho;
using nanogui::lookAt;
using nanogui::translate;
using nanogui::frustum;
using nanogui::Vector2i;
using nanogui::Vector2f;
using nanogui::Matrix4f;
using nanogui::MatrixXf;
using MatrixXu = Eigen::Matrix<uint32_t, Eigen::Dynamic, Eigen::Dynamic>;

using detail::runStatisticalTestAndOutput;

WarpVisualizationWidget::WarpVisualizationWidget(int width, int height,
                                                 std::string description)
    : nanogui::Screen(Vector2i(width, height), description)
    , m_warpAdapter(new IdentityWarpAdapter())
    , m_drawHistogram(false), m_drawGrid(true)
    , m_pointCount(0), m_lineCount(0)
    , m_testResult(false), m_testResultText("No test started.") {

    initializeVisualizerGUI();
}

bool
WarpVisualizationWidget::mouseMotionEvent(const Vector2i &p, const Vector2i & rel,
                                          int button, int modifiers) {
    if (!Screen::mouseMotionEvent(p, rel, button, modifiers)) {
        m_arcball.motion(p);
    }
    return true;
}

bool
WarpVisualizationWidget::mouseButtonEvent(const Vector2i &p, int button,
                                          bool down, int modifiers) {
    if (!Screen::mouseButtonEvent(p, button, down, modifiers)) {
        if (button == GLFW_MOUSE_BUTTON_1) {
            m_arcball.button(p, down);
            return true;
        }
    }
    return false;
}

void WarpVisualizationWidget::refresh() {
    // Generate the point positions
    pcg32 sampler;
    Eigen::MatrixXf positions;
    std::vector<Float> weights;
    m_warpAdapter->generateWarpedPoints(&sampler, m_samplingType, m_pointCount,
                                        positions, weights);

    float valueScale = 0.f;
    for (size_t i = 0; i < m_pointCount; ++i) {
        valueScale = std::max(valueScale, weights[i]);
    }
    valueScale = 1.f / valueScale;

    if (!m_warpAdapter->isIdentity()) {
        for (size_t i = 0; i < m_pointCount; ++i) {
            if (weights[i] == 0.0f) {
                positions.col(i) = Eigen::Vector3f::Constant(std::numeric_limits<float>::quiet_NaN());
                continue;
            }
            positions.col(i) =
                ((valueScale == 0 ? 1.0f : (valueScale * weights[i])) *
                 positions.col(i)) * 0.5f + Eigen::Vector3f(0.5f, 0.5f, 0.0f);
        }
    }

    // Generate a color gradient
    MatrixXf colors(3, m_pointCount);
    float colorStep = 1.f / m_pointCount;
    for (size_t i = 0; i < m_pointCount; ++i)
        colors.col(i) << i * colorStep, 1 - i * colorStep, 0;

    // Upload warped points to the GPU
    m_pointShader->bind();
    m_pointShader->uploadAttrib("position", positions);
    m_pointShader->uploadAttrib("color", colors);

    // Upload warped grid lines to the GPU
    if (m_drawGrid) {
        size_t gridRes = static_cast<size_t>(std::sqrt(static_cast<float>(m_pointCount)) + 0.5f);
        size_t fineGridRes = 16 * gridRes;
        float coarseScale = 1.f / gridRes;
        float fineScale = 1.f / fineGridRes;

        size_t idx = 0;
        m_lineCount = 4 * (gridRes+1) * (fineGridRes+1);
        positions.resize(3, m_lineCount);

        auto getPoint = [this](float x, float y) {
            auto r = m_warpAdapter->warpSample(Point2f(x, y));
            return std::make_pair(static_cast<Eigen::Matrix<float, 3, 1>>(r.first), r.second);
        };
        for (size_t i = 0; i <= gridRes; ++i) {
            for (size_t j = 0; j <= fineGridRes; ++j) {
                auto pt = getPoint(j * fineScale, i * coarseScale);
                positions.col(idx++) = valueScale == 0.f ? pt.first : (pt.first * pt.second * valueScale);
                pt = getPoint((j+1) * fineScale, i * coarseScale);
                positions.col(idx++) = valueScale == 0.f ? pt.first : (pt.first * pt.second * valueScale);
                pt = getPoint(i * coarseScale, j * fineScale);
                positions.col(idx++) = valueScale == 0.f ? pt.first : (pt.first * pt.second * valueScale);
                pt = getPoint(i * coarseScale, (j+1) * fineScale);
                positions.col(idx++) = valueScale == 0.f ? pt.first : (pt.first * pt.second * valueScale);
            }
        }
        if (!m_warpAdapter->isIdentity()) {
            for (size_t i = 0; i < m_lineCount; ++i) {
                positions.col(i) = positions.col(i) * 0.5f + Eigen::Vector3f(0.5f, 0.5f, 0.0f);
            }
        }
        m_gridShader->bind();
        m_gridShader->uploadAttrib("position", positions);
    }

    // BRDF-specific
    // int ctr = 0;
    // positions.resize(106);
    // for (int i = 0; i< = 50; ++i) {
    //     float angle1 = i * 2 * math::Pi_f / 50;
    //     float angle2 = (i+1) * 2 * math::Pi_f / 50;
    //     positions.col(ctr++) = Vector3f(std::cos(angle1)*.5f + 0.5f, std::sin(angle1)*.5f + 0.5f, 0.f);
    //     positions.col(ctr++) = Vector3f(std::cos(angle2)*.5f + 0.5f, std::sin(angle2)*.5f + 0.5f, 0.f);
    // }
    // positions.col(ctr++) = Vector3f(0.5f, 0.5f, 0.f);
    // positions.col(ctr++) = Vector3f(-m_bRec.wi.x() * 0.5f + 0.5f, -m_bRec.wi.y() * 0.5f + 0.5f, m_bRec.wi.z() * 0.5f);
    // positions.col(ctr++) = Vector3f(0.5f, 0.5f, 0.f);
    // positions.col(ctr++) = Vector3f(m_bRec.wi.x() * 0.5f + 0.5f, m_bRec.wi.y() * 0.5f + 0.5f, m_bRec.wi.z() * 0.5f);
    // m_arrowShader->bind();
    // m_arrowShader->uploadAttrib("position", positions);
}

bool WarpVisualizationWidget::runTest(double minExpFrequency, double significanceLevel) {
    std::vector<double> observedHistogram, expectedHistogram;
    size_t gridWidth = 51, gridHeight = 51;
    if (m_warpAdapter->domainDimensionality() >= 3) {
        gridWidth *= 2;
    }
    size_t nBins = gridWidth * gridHeight;

    // TODO: very, very slow (virtual function calls? lambda functions? too many Py <-> C++?)

    // Run Chi^2 test
    const auto r = runStatisticalTestAndOutput(1000 * nBins,
        gridWidth, gridHeight, m_samplingType, m_warpAdapter,
        minExpFrequency, significanceLevel, observedHistogram, expectedHistogram);
    m_testResult = r.first;
    m_testResultText = r.second;

    // Find min and max value to scale the texture
    double maxValue = 0, minValue = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < nBins; ++i) {
        maxValue = std::max(maxValue, (double) std::max(observedHistogram[i], expectedHistogram[i]));
        minValue = std::min(minValue, (double) std::min(observedHistogram[i], expectedHistogram[i]));
    }
    minValue /= 2;
    float texScale = 1 / static_cast<float>(maxValue - minValue);

    // Upload both histograms to the GPU
    std::unique_ptr<float[]> buffer(new float[nBins]);
    for (int k = 0; k < 2; ++k) {  // For each histogram
        for (size_t i = 0; i < nBins; ++i) {  // For each bin
            buffer[i] = texScale * static_cast<float>(
                (k == 0 ? observedHistogram[i] : expectedHistogram[i]) - minValue);
        }

        glBindTexture(GL_TEXTURE_2D, m_textures[k]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, gridWidth, gridHeight,
                     0, GL_RED, GL_FLOAT, buffer.get());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    return m_testResult;
}

void WarpVisualizationWidget::drawHistogram(const Vector2i &position,
                                            const Vector2i &dimensions, GLuint tex) {
    Vector2f s(- (position.x() + 0.25f) / dimensions.x(),
               - (position.y() + 0.25f) / dimensions.y());
    Vector2f e = size().array().cast<float>() / dimensions.array().cast<float>() + s.array();
    Matrix4f mvp = ortho(s.x(), e.x(), e.y(), s.y(), -1, 1);

    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    m_histogramShader->bind();
    m_histogramShader->setUniform("mvp", mvp);
    m_histogramShader->setUniform("tex", 0);
    m_histogramShader->drawIndexed(GL_TRIANGLES, 0, 2);
}

void WarpVisualizationWidget::drawGrid(const Matrix4f& mvp) {
    // Grid lines were uploaded already in `refresh`
    m_gridShader->bind();
    m_gridShader->setUniform("mvp", mvp);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_gridShader->drawArray(GL_LINES, 0, m_lineCount);
    glDisable(GL_BLEND);
}

void WarpVisualizationWidget::drawContents() {
    // Set up a perspective camera matrix
    Matrix4f view, proj, model;
    view = lookAt(Vector3f(0, 0, 2), Vector3f(0, 0, 0), Vector3f(0, 1, 0));
    const float viewAngle = 30, near = 0.01, far = 100;
    float fH = std::tan(viewAngle / 360.0f * math::Pi_f) * near;
    float fW = fH * (float) mSize.x() / (float) mSize.y();
    proj = frustum(-fW, fW, -fH, fH, near, far);

    model.setIdentity();
    model = translate(model, Vector3f(-0.5f, -0.5f, 0.0f));
    model = m_arcball.matrix() * model;

    if (m_drawHistogram) {
        // Render the histograms
        const int spacer = 20;
        const int histWidth = (width() - 3*spacer) / 2;
        int histHeight = histWidth;
        if (m_warpAdapter->domainDimensionality() >= 3) {
            histHeight /= 2;
        }
        const int verticalOffset = (height() - histHeight) / 2;

        drawHistogram(Vector2i(spacer, verticalOffset), Vector2i(histWidth, histHeight), m_textures[0]);
        drawHistogram(Vector2i(2*spacer + histWidth, verticalOffset), Vector2i(histWidth, histHeight), m_textures[1]);

        auto ctx = nvgContext();
        nvgBeginFrame(ctx, mSize[0], mSize[1], mPixelRatio);
        nvgBeginPath(ctx);
        nvgRect(ctx, spacer, verticalOffset + histHeight + spacer, width()-2*spacer, 70);
        nvgFillColor(ctx, m_testResult ? Color(100, 255, 100, 100) : Color(255, 100, 100, 100));
        nvgFill(ctx);
        nvgFontSize(ctx, 24.0f);
        nvgFontFace(ctx, "sans-bold");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(ctx, Color(255, 255));
        nvgText(ctx, spacer + histWidth / 2, verticalOffset - 3 * spacer,
                "Sample histogram", nullptr);
        nvgText(ctx, 2 * spacer + (histWidth * 3) / 2, verticalOffset - 3 * spacer,
                "Integrated density", nullptr);
        nvgStrokeColor(ctx, Color(255, 255));
        nvgStrokeWidth(ctx, 2);
        nvgBeginPath(ctx);
        nvgRect(ctx, spacer, verticalOffset, histWidth, histHeight);
        nvgRect(ctx, 2 * spacer + histWidth, verticalOffset, histWidth,
                histHeight);
        nvgStroke(ctx);
        nvgFontSize(ctx, 20.0f);
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);

        float bounds[4];
        nvgTextBoxBounds(ctx, 0, 0, width() - 2 * spacer,
                         m_testResultText.c_str(), nullptr, bounds);
        nvgTextBox(
            ctx, spacer, verticalOffset + histHeight + spacer + (70 - bounds[3])/2,
            width() - 2 * spacer, m_testResultText.c_str(), nullptr);
        nvgEndFrame(ctx);
    } else {
        // Render the point set
        Matrix4f mvp = proj * view * model;
        m_pointShader->bind();
        m_pointShader->setUniform("mvp", mvp);
        glPointSize(2);
        glEnable(GL_DEPTH_TEST);
        m_pointShader->drawArray(GL_POINTS, 0, m_pointCount);

        if (m_drawGrid) {
            drawGrid(mvp);
        }
    }
}

void WarpVisualizationWidget::initializeVisualizerGUI() {
    m_pointShader.reset(new GLShader());
    m_pointShader->init("Point shader",
                        kPointVertexShader,
                        kPointFragmentShader);

    m_gridShader.reset(new GLShader());
    m_gridShader->init("Grid shader",
                        kGridVertexShader,
                        kGridFragmentShader);

    m_arrowShader.reset(new GLShader());
    m_arrowShader->init("Arrow shader",
                        kArrowVertexShader,
                        kArrowFragmentShader);

    m_histogramShader.reset(new GLShader());
    m_histogramShader->init("Histogram shader",
                            kHistogramVertexShader,
                            kHistogramFragmentShader);

    // Initially, upload a single uniform rectangle to the histogram
    MatrixXf positions(2, 4);
    MatrixXu indices(3, 2);
    positions <<
        0, 1, 1, 0,
        0, 0, 1, 1;
    indices <<
        0, 2,
        1, 3,
        2, 0;
    m_histogramShader->bind();
    m_histogramShader->uploadAttrib("position", positions);
    m_histogramShader->uploadIndices(indices);

    glGenTextures(2, &m_textures[0]);
    glBindTexture(GL_TEXTURE_2D, m_textures[0]);

    setBackground(Vector3f(0, 0, 0));
    drawContents();

    framebufferSizeChanged();
}

NAMESPACE_END(mitsuba)
NAMESPACE_END(warp)

