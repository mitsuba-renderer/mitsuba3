#include <mitsuba/core/math.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/warp.h>

#include <hypothesis.h>
#include <nanogui/nanogui.h>
#include <nanogui/glutil.h>
#include <pcg32.h>
#include "python.h"

using VectorList = std::vector<Vector3f>;

/// Enum of available warp types
enum WarpType {
    NoWarp = 0,
    UniformSphere,
    UniformHemisphere,
    CosineHemisphere,
    UniformCone,
    UniformDisk,
    UniformDiskConcentric,
    UniformTriangle,
    StandardNormal,
    UniformTent,
    NonUniformTent
};

/// Enum of available point sampling types
enum SamplingType {
    Independent = 0,
    Grid,
    Stratified
};

namespace {

std::pair<Vector3f, Float> warpPoint(WarpType warpType, Point2f sample, Float parameterValue) {
    Vector3f result;

    auto fromPoint = [](const Point2f& p) {
        Vector3f v;
        v[0] = p[0]; v[1] = p[1]; v[2] = 0.0;
        return v;
    };

    switch (warpType) {
        case NoWarp:
            result = fromPoint(sample);
            break;
        case UniformSphere:
            result = warp::squareToUniformSphere(sample);
            break;
        case UniformHemisphere:
            result = warp::squareToUniformHemisphere(sample);
            break;
        case CosineHemisphere:
            result = warp::squareToCosineHemisphere(sample);
            break;
        case UniformCone:
            result = warp::squareToUniformCone(parameterValue, sample);
            break;

        case UniformDisk:
            result = fromPoint(warp::squareToUniformDisk(sample));
            break;
        case UniformDiskConcentric:
            result = fromPoint(warp::squareToUniformDiskConcentric(sample));
            break;
        case UniformTriangle:
            result = fromPoint(warp::squareToUniformTriangle(sample));
            break;
        case StandardNormal:
            result = fromPoint(warp::squareToStdNormal(sample));
            break;
        case UniformTent:
            result = fromPoint(warp::squareToTent(sample));
            break;
        // TODO: need to pass 3 parameters instead of one...
        // case NonUniformTent:
        //     result << warp::squareToUniformSphere(sample);
        //     break;
        default:
            throw new std::runtime_error("Unsupported warping function");
    }
    return std::make_pair(result, 1.f);
}

Point2f domainToPoint(const Vector3f &v, WarpType warpType) {
    Point2f p;
    switch (warpType) {
        case NoWarp:
            p[0] = v[0]; p[1] = v[1];
            break;
        case UniformDisk:
        case UniformDiskConcentric:
            p[0] = 0.5f * v[0] + 0.5f;
            p[1] = 0.5f * v[1] + 0.5f;
            break;

        case StandardNormal:
            p = 5 * domainToPoint(v, UniformDisk);
            break;

        default:
            p[0] = std::atan2(v[1], v[0]) * math::InvTwoPi;
            if (p[0] < 0) p[0] += 1;
            p[1] = 0.5f * v[2] + 0.5f;
            break;
    }

    return p;
}

double getPdfScalingFactor(WarpType warpType) {
    switch (warpType) {
        case NoWarp: return 1.0;

        case UniformDisk:
        case UniformDiskConcentric:
            return 4.0;

        case StandardNormal: return 100.0;  // TODO: why?

        case UniformSphere:
        case UniformHemisphere:
        case CosineHemisphere:
        default:
            return 4.0 * math::Pi_d;
    }
}

Float pdfValueForSample(WarpType warpType, double x, double y, Float parameterValue) {
    Point2f p;
    switch (warpType) {
        case NoWarp: return 1.0;

        // TODO: need to use domain indicator functions to zero-out where appropriate
        case UniformDisk:
            p[0] = 2 * x - 1; p[1] = 2 * y - 1;
            return warp::squareToUniformDiskPdf();
        case UniformDiskConcentric:
            p[0] = 2 * x - 1; p[1] = 2 * y - 1;
            return warp::squareToUniformDiskConcentricPdf();

        case StandardNormal:
            p[0] = 2 * x - 1; p[1] = 2 * y - 1;
            return warp::squareToStdNormalPdf((1 / 5.0) * p);

        // TODO
        // case UniformTriangle:
        // case UniformTent:
        // case NonUniformTent:

        default: {
            x = 2 * math::Pi_d * x;
            y = 2 * y - 1;

            double sinTheta = std::sqrt(1 - y * y);
            double sinPhi, cosPhi;
            math::sincos(x, &sinPhi, &cosPhi);

            Vector3f v((Float) (sinTheta * cosPhi),
                       (Float) (sinTheta * sinPhi),
                       (Float) y);

            switch (warpType) {
                case UniformSphere:
                    return warp::squareToUniformSpherePdf();
                case UniformHemisphere:
                    return warp::squareToUniformHemispherePdf();
                case CosineHemisphere:
                    Log(EError, "TODO: squareToCosineHemispherePdf");
                    // return warp::squareToCosineHemispherePdf();
                case UniformCone:
                    return warp::squareToUniformConePdf(parameterValue);
                default:
                    Log(EError, "Unsupported warp type");
            }
        }
    }

    return 0.0;
}

}  // end anonymous namespace

/**
 * \warning The point count may change depending on the sample strategy,
 *          the effective point count is the length of \ref positions
 *          after the function has returned.
 */
void generatePoints(size_t &pointCount, SamplingType pointType, WarpType warpType,
                    Float parameterValue,
                    VectorList &positions, std::vector<Float> &weights) {
    /* Determine the number of points that should be sampled */
    size_t sqrtVal = static_cast<size_t>(std::sqrt((float) pointCount) + 0.5f);
    float invSqrtVal = 1.f / sqrtVal;
    if (pointType == Grid || pointType == Stratified)
        pointCount = sqrtVal*sqrtVal;

    pcg32 rng;

    for (size_t i = 0; i < pointCount; ++i) {
        size_t y = i / sqrtVal, x = i % sqrtVal;
        Point2f sample;

        switch (pointType) {
            case Independent:
                sample = Point2f(rng.nextFloat(), rng.nextFloat());
                break;

            case Grid:
                sample = Point2f((x + 0.5f) * invSqrtVal, (y + 0.5f) * invSqrtVal);
                break;

            case Stratified:
                sample = Point2f((x + rng.nextFloat()) * invSqrtVal,
                                 (y + rng.nextFloat()) * invSqrtVal);
                break;
        }

        auto result = warpPoint(warpType, sample, parameterValue);
        positions.push_back(result.first);
        weights.push_back(result.second);
    }
}

std::vector<double> computeHistogram(WarpType warpType,
                                    const VectorList &positions,
                                    const std::vector<Float> &weights,
                                    size_t gridWidth, size_t gridHeight) {
    std::vector<double> hist(gridWidth * gridHeight, 0.0);

    for (size_t i = 0; i < positions.size(); ++i) {
        if (weights[i] == 0) {
            continue;
        }

        Point2f sample = domainToPoint(positions[i], warpType);
        float x = sample[0],
              y = sample[1];

        size_t xbin = std::min(gridWidth - 1,
            std::max(0lu, static_cast<size_t>(std::floor(x * gridWidth))));
        size_t ybin = std::min(gridHeight - 1,
            std::max(0lu, static_cast<size_t>(std::floor(y * gridHeight))));

        hist[ybin * gridWidth + xbin] += 1;
    }

    return hist;
}

std::vector<double> generateExpectedHistogram(size_t pointCount,
                                             WarpType warpType, Float parameterValue,
                                             size_t gridWidth, size_t gridHeight) {
    std::vector<double> hist(gridWidth * gridHeight, 0.0);
    double scale = pointCount * getPdfScalingFactor(warpType);

    auto integrand = [&warpType, &parameterValue](double x, double y) {
        return pdfValueForSample(warpType, x, y, parameterValue);
    };

    for (size_t y = 0; y < gridHeight; ++y) {
        double yStart = y       / static_cast<double>(gridHeight);
        double yEnd   = (y + 1) / static_cast<double>(gridHeight);
        for (size_t x = 0; x < gridWidth; ++x) {
            double xStart =  x      / static_cast<double>(gridWidth);
            double xEnd   = (x + 1) / static_cast<double>(gridWidth);

            hist[y * gridWidth + x] = scale *
                hypothesis::adaptiveSimpson2D(integrand,
                                              yStart, xStart, yEnd, xEnd);
            if (hist[y * gridWidth + x] < 0)
                Log(EError, "The Pdf() function returned negative values!");
        }
    }

    return hist;
}

std::pair<bool, std::string>
runStatisticalTest(size_t pointCount, size_t gridWidth, size_t gridHeight,
                   SamplingType samplingType, WarpType warpType, Float parameterValue,
                   double minExpFrequency, double significanceLevel) {
    const auto nBins = gridWidth * gridHeight;
    VectorList positions;
    std::vector<Float> weights;

    generatePoints(pointCount, samplingType, warpType,
                   parameterValue, positions, weights);
    auto observedHistogram = computeHistogram(warpType, positions, weights,
                                              gridWidth, gridHeight);
    auto expectedHistogram = generateExpectedHistogram(pointCount,
                                                       warpType, parameterValue,
                                                       gridWidth, gridHeight);

    return hypothesis::chi2_test(nBins, observedHistogram.data(), expectedHistogram.data(),
                                 pointCount, minExpFrequency, significanceLevel, 1);
}

namespace warp_detail {
using nanogui::Arcball;
using nanogui::Color;
using nanogui::GLShader;
using nanogui::ortho;
using nanogui::lookAt;
using nanogui::translate;
using nanogui::frustum;
using Point2i = Eigen::Matrix<int, 2, 1>;
using nanogui::Vector2i; // = Eigen::Matrix<int, 2, 1>;
using nanogui::Vector2f; // = Eigen::Matrix<float, 2, 1>;
using nanogui::Matrix4f; // = Eigen::Matrix<Float, 4, 4>;
using nanogui::MatrixXf; // = Eigen::Matrix<Float, Eigen::Dynamic, Eigen::Dynamic>;
using MatrixXi = Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic>;

// TODO: move this class out of the way
class WarpVisualizationWidget : public nanogui::Screen {

public:
    WarpVisualizationWidget()
        : nanogui::Screen(Vector2i(800, 600), "Warp visualization")
        , m_drawHistogram(false), m_drawGrid(true)
        , m_pointCount(0), m_lineCount(0)
        , m_testResult(false), m_testResultText("No test started.") {
        Log(EInfo, "instantiated :)");
        initializeVisualizerGUI();
    }

    ~WarpVisualizationWidget() {
        glDeleteTextures(2, &m_textures[0]);
        delete m_pointShader;
        delete m_gridShader;
        delete m_arrowShader;
        delete m_histogramShader;
    }

    void framebufferSizeChanged() {
        m_arcball.setSize(mSize);
    }

    virtual bool
    mouseMotionEvent(const Vector2i &p, const Vector2i &, int, int) override {
        m_arcball.motion(p);
        return true;
    }

    virtual bool
    mouseButtonEvent(const Vector2i &p, int button, bool down, int) override {
        if (button == GLFW_MOUSE_BUTTON_1) {
            m_arcball.button(p, down);
        }
        return true;
    }

    void drawHistogram(const Point2i &pos_, const Vector2i &size_, GLuint tex) {
        Vector2f s = -(pos_.array().cast<float>() + Vector2f(0.25f, 0.25f).array())  / size_.array().cast<float>();
        Vector2f e = size().array().cast<float>() / size_.array().cast<float>() + s.array();
        Matrix4f mvp = ortho(s.x(), e.x(), e.y(), s.y(), -1, 1);

        glDisable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        m_histogramShader->bind();
        m_histogramShader->setUniform("mvp", mvp);
        m_histogramShader->setUniform("tex", 0);
        m_histogramShader->drawIndexed(GL_TRIANGLES, 0, 2);
    }

    virtual void drawContents() override {
        /* Set up a perspective camera matrix */
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
            /* Render the histograms */
            const int spacer = 20;
            const int histWidth = (width() - 3*spacer) / 2;
            const int histHeight = histWidth;
            // TODO: histHeight = (warpType == None || warpType == Disk) ? histWidth : histWidth / 2;
            const int verticalOffset = (height() - histHeight) / 2;

            drawHistogram(Point2i(spacer, verticalOffset), Vector2i(histWidth, histHeight), m_textures[0]);
            drawHistogram(Point2i(2*spacer + histWidth, verticalOffset), Vector2i(histWidth, histHeight), m_textures[1]);

            auto ctx = nvgContext();
            // TODO: need getter for Screen::mPixelRatio
            const double pixelRatio = 1.0;
            nvgBeginFrame(ctx, mSize[0], mSize[1], pixelRatio);
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
            /* Render the point set */
            Matrix4f mvp = proj * view * model;
            m_pointShader->bind();
            m_pointShader->setUniform("mvp", mvp);
            glPointSize(2);
            glEnable(GL_DEPTH_TEST);
            m_pointShader->drawArray(GL_POINTS, 0, m_pointCount);

            if (m_drawGrid) {
                m_gridShader->bind();
                m_gridShader->setUniform("mvp", mvp);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                m_gridShader->drawArray(GL_LINES, 0, m_lineCount);
                glDisable(GL_BLEND);
            }
        }
    }

    void initializeVisualizerGUI() {
        m_pointShader = new GLShader();
        m_pointShader->init(
            "Point shader",

            /* Vertex shader */
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
            "}",

            /* Fragment shader */
            "#version 330\n"
            "in vec3 frag_color;\n"
            "out vec4 out_color;\n"
            "void main() {\n"
            "    if (frag_color == vec3(0.0))\n"
            "        discard;\n"
            "    out_color = vec4(frag_color, 1.0);\n"
            "}"
        );

        m_gridShader = new GLShader();
        m_gridShader->init(
            "Grid shader",

            /* Vertex shader */
            "#version 330\n"
            "uniform mat4 mvp;\n"
            "in vec3 position;\n"
            "void main() {\n"
            "    gl_Position = mvp * vec4(position, 1.0);\n"
            "}",

            /* Fragment shader */
            "#version 330\n"
            "out vec4 out_color;\n"
            "void main() {\n"
            "    out_color = vec4(vec3(1.0), 0.4);\n"
            "}"
        );

        m_arrowShader = new GLShader();
        m_arrowShader->init(
            "Arrow shader",

            /* Vertex shader */
            "#version 330\n"
            "uniform mat4 mvp;\n"
            "in vec3 position;\n"
            "void main() {\n"
            "    gl_Position = mvp * vec4(position, 1.0);\n"
            "}",

            /* Fragment shader */
            "#version 330\n"
            "out vec4 out_color;\n"
            "void main() {\n"
            "    out_color = vec4(vec3(1.0), 0.4);\n"
            "}"
        );

        m_histogramShader = new GLShader();
        m_histogramShader->init(
            "Histogram shader",

            /* Vertex shader */
            "#version 330\n"
            "uniform mat4 mvp;\n"
            "in vec2 position;\n"
            "out vec2 uv;\n"
            "void main() {\n"
            "    gl_Position = mvp * vec4(position, 0.0, 1.0);\n"
            "    uv = position;\n"
            "}",

            /* Fragment shader */
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
            "}"
        );

        // Initially, upload a single uniform rectangle to the histogram
        MatrixXf positions(2, 4);
        MatrixXi indices(3, 2);
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

        // mBackground.setZero();
        drawContents();

        framebufferSizeChanged();
    }

    void hello() {
        Log(EInfo, "says hello!");
        drawContents();
    }

private:
    GLShader *m_pointShader = nullptr;
    GLShader *m_gridShader = nullptr;
    GLShader *m_histogramShader = nullptr;
    GLShader *m_arrowShader = nullptr;
    GLuint m_textures[2];
    Arcball m_arcball;

    bool m_drawHistogram, m_drawGrid;
    int m_pointCount, m_lineCount;
    WarpType m_warpType;
    bool m_testResult;
    std::string m_testResultText;
};

}  // end namespace detail

MTS_PY_EXPORT(warp) {
    auto m2 = m.def_submodule("warp", "Common warping techniques that map from the unit"
                                      "square to other domains, such as spheres,"
                                      " hemispheres, etc.");

    m2.mdef(warp, squareToUniformSphere)
      .mdef(warp, squareToUniformSpherePdf)
      .def("unitSphereIndicator", [](const Vector3f& v) {
        return (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) <= 1;
      }, "Indicator function of the 3D unit sphere's domain.")

      .mdef(warp, squareToUniformHemisphere)
      .mdef(warp, squareToUniformHemispherePdf)
      .def("unitHemisphereIndicator", [](const Vector3f& v) {
        return ((v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) <= 1)
            && (v[2] >= 0);
      }, "Indicator function of the 3D unit sphere's domain.")

      .mdef(warp, squareToCosineHemisphere)
      // TODO: cosine hemisphere's PDF

      .mdef(warp, squareToUniformCone)
      .mdef(warp, squareToUniformConePdf)
      // TODO
      // .def("unitConeIndicator", [](const Vector3f& v, float cosCutoff) {
      //   return false;
      // }, "Indicator function of the 3D cone's domain.")

      .mdef(warp, squareToUniformDisk)
      .mdef(warp, squareToUniformDiskPdf)
      .def("unitDiskIndicator", [](const Point2f& p) {
        return (p[0] * p[0] + p[1] * p[1]) <= 1;
      }, "Indicator function of the 2D unit disc's domain.")

      .mdef(warp, squareToUniformDiskConcentric)
      .mdef(warp, squareToUniformDiskConcentricPdf)
      .mdef(warp, uniformDiskToSquareConcentric)

      .mdef(warp, squareToUniformTriangle)

      .mdef(warp, squareToStdNormal)
      .mdef(warp, squareToStdNormalPdf)

      .mdef(warp, squareToTent)
      .mdef(warp, intervalToNonuniformTent);


    py::enum_<WarpType>(m2, "WarpType")
        .value("NoWarp", WarpType::NoWarp)
        .value("UniformSphere", WarpType::UniformSphere)
        .value("UniformHemisphere", WarpType::UniformHemisphere)
        .value("CosineHemisphere", WarpType::CosineHemisphere)
        .value("UniformCone", WarpType::UniformCone)
        .value("UniformDisk", WarpType::UniformDisk)
        .value("UniformDiskConcentric", WarpType::UniformDiskConcentric)
        .value("UniformTriangle", WarpType::UniformTriangle)
        .value("StandardNormal", WarpType::StandardNormal)
        .value("UniformTent", WarpType::UniformTent)
        .value("NonUniformTent", WarpType::NonUniformTent)
        .export_values();

    py::enum_<SamplingType>(m2, "SamplingType")
        .value("Independent", SamplingType::Independent)
        .value("Grid", SamplingType::Grid)
        .value("Stratified", SamplingType::Stratified)
        .export_values();


    m2.def("generatePoints", [](size_t pointCount, SamplingType pointType,
                                WarpType warpType, Float parameterValue) {
        VectorList points;
        std::vector<Float> weights;
        generatePoints(pointCount, pointType,
                       warpType, parameterValue,
                       points, weights);
        return std::make_pair(points, weights);
    }, "Generate and warp points. Returns (list of points, list of weights)");

    m2.def("computeHistogram", &computeHistogram,
           "Compute histogram from warped points.");

    m2.def("generateExpectedHistogram", &generateExpectedHistogram,
           "Generate the theoretical histogram from the PDF of the warping function.");

    m2.def("runStatisticalTest", &runStatisticalTest,
           "Runs a Chi^2 statistical test verifying the given warping type"
           "against its PDF. Returns (passed, reason)");

    // Warp visualization widget
    using warp_detail::WarpVisualizationWidget;
    py::class_<WarpVisualizationWidget>(m2, "WarpVisualizationWidget")
        .def(py::init<>(), "Default constructor.")
        .def("hello", &WarpVisualizationWidget::hello, "Prints hello")
        .def("setVisible", [](WarpVisualizationWidget &widget, bool v) {
            widget.setVisible(v);
         }, "");
}
