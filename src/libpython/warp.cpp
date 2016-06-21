#include <mitsuba/core/math.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/warp.h>

#include <hypothesis.h>
#include <nanogui/nanogui.h>
#include <nanogui/glutil.h>
#include <pcg32.h>
#include "python.h"

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

// TODO: proper support for:
// - uniformDiskToSquareConcentric
// - intervalToNonuniformTent

namespace {

bool isTwoDimensionalWarp(WarpType warpType) {
    switch (warpType) {
        case NoWarp:
        case UniformDisk:
        case UniformDiskConcentric:
        case UniformTriangle:
        case StandardNormal:
        case UniformTent:
            return true;
        default:
            return false;
    }
}

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

Point2f domainToPoint(const Eigen::Vector3f &v, WarpType warpType) {
    Point2f p;
    switch (warpType) {
        case NoWarp:
            p[0] = v(0); p[1] = v(1);
            break;
        case UniformDisk:
        case UniformDiskConcentric:
            p[0] = 0.5f * v(0) + 0.5f;
            p[1] = 0.5f * v(1) + 0.5f;
            break;

        case StandardNormal:
            p = 5 * domainToPoint(v, UniformDisk);
            break;

        default:
            p[0] = std::atan2(v(1), v(0)) * math::InvTwoPi;
            if (p[0] < 0) p[0] += 1;
            p[1] = 0.5f * v(2) + 0.5f;
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

        // TODO: probably not correct
        case StandardNormal: return 100.0;

        case UniformSphere:
        case UniformHemisphere:
        case CosineHemisphere:
        default:
            return 4.0 * math::Pi_d;
    }
}

Float pdfValueForSample(WarpType warpType, double x, double y, Float parameterValue) {
    if (warpType == NoWarp) {
        return 1.0;
    } else if (isTwoDimensionalWarp(warpType)) {
        Point2f p;
        p[0] = 2 * x - 1; p[1] = 2 * y - 1;

        switch (warpType) {
            case UniformDisk:
                return warp::unitDiskIndicator(p) * warp::squareToUniformDiskPdf();
            case UniformDiskConcentric:
                return warp::unitDiskIndicator(p) * warp::squareToUniformDiskConcentricPdf();
            case StandardNormal:
                // TODO: do not hardcode domain
                return warp::squareToStdNormalPdf((1 / 5.0) * p);
            case UniformTriangle:
                return warp::triangleIndicator(p) * warp::squareToUniformTrianglePdf(p);
            case UniformTent:
                return warp::squareToTentPdf(p);

            default:
                Log(EError, "Unsupported 2D warp type");
        }
    } else {
        // Map 2D sample to 3D domain
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
                return warp::unitSphereIndicator(v) * warp::squareToUniformSpherePdf();
            case UniformHemisphere:
                return warp::unitHemisphereIndicator(v) * warp::squareToUniformHemispherePdf();
            case CosineHemisphere:
                return warp::unitHemisphereIndicator(v) * warp::squareToCosineHemispherePdf(v);
            case UniformCone:
                return warp::unitConeIndicator(v) * warp::squareToUniformConePdf(parameterValue);
            default:
                Log(EError, "Unsupported 3D warp type");
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
                    Eigen::MatrixXf &positions, std::vector<Float> &weights) {
    /* Determine the number of points that should be sampled */
    size_t sqrtVal = static_cast<size_t>(std::sqrt((float) pointCount) + 0.5f);
    float invSqrtVal = 1.f / sqrtVal;
    if (pointType == Grid || pointType == Stratified)
        pointCount = sqrtVal*sqrtVal;

    pcg32 rng;

    positions.resize(3, pointCount);
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
        positions.col(i) = static_cast<Eigen::Matrix<float, 3, 1>>(result.first);
        weights.push_back(result.second);
    }
}

std::vector<double> computeHistogram(WarpType warpType,
                                    const Eigen::MatrixXf &positions,
                                    const std::vector<Float> &weights,
                                    size_t gridWidth, size_t gridHeight) {
    std::vector<double> hist(gridWidth * gridHeight, 0.0);

    for (size_t i = 0; i < static_cast<size_t>(positions.cols()); ++i) {
        if (weights[i] == 0) {
            continue;
        }

        Point2f sample = domainToPoint(positions.col(i), warpType);
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

    auto integrand = [&warpType, &parameterValue](double y, double x) {
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

std::pair<bool, std::string> runStatisticalTestAndOutput(size_t pointCount,
    size_t gridWidth, size_t gridHeight, SamplingType samplingType, WarpType warpType,
    Float parameterValue, double minExpFrequency, double significanceLevel,
    std::vector<double> &observedHistogram, std::vector<double> &expectedHistogram) {

    const auto nBins = gridWidth * gridHeight;
    Eigen::MatrixXf positions;
    std::vector<Float> weights;

    generatePoints(pointCount, samplingType, warpType,
                   parameterValue, positions, weights);
    observedHistogram = computeHistogram(warpType, positions, weights,
                                         gridWidth, gridHeight);
    expectedHistogram = generateExpectedHistogram(pointCount,
                                                  warpType, parameterValue,
                                                  gridWidth, gridHeight);

    return hypothesis::chi2_test(nBins, observedHistogram.data(), expectedHistogram.data(),
                                 pointCount, minExpFrequency, significanceLevel, 1);
}

std::pair<bool, std::string>
runStatisticalTest(size_t pointCount, size_t gridWidth, size_t gridHeight,
                   SamplingType samplingType, WarpType warpType, Float parameterValue,
                   double minExpFrequency, double significanceLevel) {
    std::vector<double> observedHistogram, expectedHistogram;
    return runStatisticalTestAndOutput(pointCount,
        gridWidth, gridHeight, samplingType, warpType, parameterValue,
        minExpFrequency, significanceLevel, observedHistogram, expectedHistogram);
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
using MatrixXu = Eigen::Matrix<uint32_t, Eigen::Dynamic, Eigen::Dynamic>;

// TODO: move this class out of the way
class WarpVisualizationWidget : public nanogui::Screen {

public:
    WarpVisualizationWidget(int width, int height, std::string description)
        : nanogui::Screen(Vector2i(width, height), description)
        , m_drawHistogram(false), m_drawGrid(true)
        , m_pointCount(0), m_lineCount(0)
        , m_testResult(false), m_testResultText("No test started.") {
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
    mouseMotionEvent(const Vector2i &p, const Vector2i & rel,
                     int button, int modifiers) override {
        if (!Screen::mouseMotionEvent(p, rel, button, modifiers)) {
            m_arcball.motion(p);
        }
        return true;
    }

    virtual bool
    mouseButtonEvent(const Vector2i &p, int button,
                     bool down, int modifiers) override {
        if (down && isDrawingHistogram()) {
            setDrawHistogram(false);
            window->setVisible(true);
            return true;
        }
        if (!Screen::mouseButtonEvent(p, button, down, modifiers)) {
            if (button == GLFW_MOUSE_BUTTON_1) {
                m_arcball.button(p, down);
                return true;
            }
        }
        return false;
    }

    virtual bool
    keyboardEvent(int key, int scancode, int action, int modifiers) override {
        if (Screen::keyboardEvent(key, scancode, action, modifiers)) {
            return true;
        }
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            setVisible(false);
            return true;
        }
        return false;
    }

    virtual void refresh() {
        // Generate the point positions
        Eigen::MatrixXf positions;
        std::vector<Float> values;
        generatePoints(m_pointCount, m_samplingType, m_warpType, m_parameterValue,
                       positions, values);

        float valueScale = 0.f;
        for (size_t i = 0; i < m_pointCount; ++i) {
            valueScale = std::max(valueScale, values[i]);
        }
        valueScale = 1.f / valueScale;

        if (m_warpType != NoWarp) {
            for (size_t i = 0; i < m_pointCount; ++i) {
                if (values[i] == 0.0f) {
                    positions.col(i) = Eigen::Vector3f::Constant(std::numeric_limits<float>::quiet_NaN());
                    continue;
                }
                positions.col(i) =
                    ((valueScale == 0 ? 1.0f : (valueScale * values[i])) *
                     positions.col(i)) * 0.5f + Eigen::Vector3f(0.5f, 0.5f, 0.0f);
            }
        }

        // Generate a color gradient
        MatrixXf colors(3, m_pointCount);
        float colorStep = 1.f / m_pointCount;
        for (size_t i = 0; i < m_pointCount; ++i)
            colors.col(i) << i * colorStep, 1 - i * colorStep, 0;

        // Upload points to GPU
        m_pointShader->bind();
        m_pointShader->uploadAttrib("position", positions);
        m_pointShader->uploadAttrib("color", colors);

        // Upload grid lines to the GPU
        if (m_drawGrid) {
            size_t gridRes = static_cast<size_t>(std::sqrt(static_cast<float>(m_pointCount)) + 0.5f);
            size_t fineGridRes = 16 * gridRes;
            float coarseScale = 1.f / gridRes;
            float fineScale = 1.f / fineGridRes;

            size_t idx = 0;
            m_lineCount = 4 * (gridRes+1) * (fineGridRes+1);
            positions.resize(3, m_lineCount);

            auto getPoint = [this, &fineScale, &coarseScale](float x, float y) {
                auto r = warpPoint(m_warpType, Point2f(x, y), m_parameterValue);
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
            if (m_warpType != NoWarp) {
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

    bool runTest(double minExpFrequency, double significanceLevel) {
        std::vector<double> observedHistogram, expectedHistogram;
        size_t gridWidth = 51, gridHeight = 51;
        if (!isTwoDimensionalWarp(m_warpType)) {
            gridWidth *= 2;
        }
        size_t nBins = gridWidth * gridHeight;

        // Run Chi^2 test
        const auto r = runStatisticalTestAndOutput(1000 * nBins,
            gridWidth, gridHeight, m_samplingType, m_warpType, m_parameterValue,
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

    void drawGrid(const Matrix4f& mvp) {
        // Grid lines were uploaded already in `refresh`
        m_gridShader->bind();
        m_gridShader->setUniform("mvp", mvp);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        m_gridShader->drawArray(GL_LINES, 0, m_lineCount);
        glDisable(GL_BLEND);
    }

    virtual void drawContents() override {
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
            if (!isTwoDimensionalWarp(m_warpType)) {
                histHeight /= 2;
            }
            const int verticalOffset = (height() - histHeight) / 2;

            drawHistogram(Point2i(spacer, verticalOffset), Vector2i(histWidth, histHeight), m_textures[0]);
            drawHistogram(Point2i(2*spacer + histWidth, verticalOffset), Vector2i(histWidth, histHeight), m_textures[1]);

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

    void setSamplingType(SamplingType s) { m_samplingType = s; }
    void setWarpType(WarpType w) { m_warpType = w; }
    void setParameterValue(float v) { m_parameterValue = v; }
    void setPointCount(int n) { m_pointCount = n; }

    bool isDrawingHistogram() { return m_drawHistogram; }
    void setDrawHistogram(bool draw) { m_drawHistogram = draw; }
    bool isDrawingGrid() { return m_drawGrid; }
    void setDrawGrid(bool draw) { m_drawGrid = draw; }

    nanogui::Window *window;

private:
    GLShader *m_pointShader = nullptr;
    GLShader *m_gridShader = nullptr;
    GLShader *m_histogramShader = nullptr;
    GLShader *m_arrowShader = nullptr;
    GLuint m_textures[2];
    Arcball m_arcball;

    bool m_drawHistogram, m_drawGrid;
    size_t m_pointCount, m_lineCount;
    SamplingType m_samplingType;
    WarpType m_warpType;
    float m_parameterValue;
    bool m_testResult;
    std::string m_testResultText;
};

}  // end namespace detail

MTS_PY_EXPORT(warp) {
    auto m2 = m.def_submodule("warp", "Common warping techniques that map from the unit"
                                      "square to other domains, such as spheres,"
                                      " hemispheres, etc.");

    m2.mdef(warp, unitSphereIndicator)
      .mdef(warp, squareToUniformSphere)
      .mdef(warp, squareToUniformSpherePdf)

      .mdef(warp, unitHemisphereIndicator)
      .mdef(warp, squareToUniformHemisphere)
      .mdef(warp, squareToUniformHemispherePdf)

      .mdef(warp, squareToCosineHemisphere)
      .mdef(warp, squareToCosineHemispherePdf)

      .mdef(warp, unitConeIndicator)
      .mdef(warp, squareToUniformCone)
      .mdef(warp, squareToUniformConePdf)

      .mdef(warp, unitDiskIndicator)
      .mdef(warp, squareToUniformDisk)
      .mdef(warp, squareToUniformDiskPdf)

      .mdef(warp, squareToUniformDiskConcentric)
      .mdef(warp, squareToUniformDiskConcentricPdf)

      .mdef(warp, unitSquareIndicator)
      .mdef(warp, uniformDiskToSquareConcentric)

      .mdef(warp, triangleIndicator)
      .mdef(warp, squareToUniformTriangle)
      .mdef(warp, squareToUniformTrianglePdf)

      .mdef(warp, squareToStdNormal)
      .mdef(warp, squareToStdNormalPdf)

      .mdef(warp, squareToTent)
      .mdef(warp, squareToTentPdf)
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

    m2.def("runStatisticalTest", &runStatisticalTest,
           "Runs a Chi^2 statistical test verifying the given warping type"
           "against its PDF. Returns (passed, reason)");

    // Warp visualization widget, inherits from nanogui::Screen which
    // is already exposed to Python in another module.
    using warp_detail::WarpVisualizationWidget;
    const auto PyScreen = static_cast<py::object>(py::module::import("nanogui").attr("Screen"));
    py::class_<WarpVisualizationWidget>(m2, "WarpVisualizationWidget", PyScreen)
        .def(py::init<int, int, std::string>(), "Default constructor.")
        .def("runTest", &WarpVisualizationWidget::runTest,
            "Run the Chi^2 test for the selected parameters and displays the histograms.")
        .def("refresh", &WarpVisualizationWidget::refresh, "Should be called upon UI interaction.")
        .def("setSamplingType", &WarpVisualizationWidget::setSamplingType, "")
        .def("setWarpType", &WarpVisualizationWidget::setWarpType, "")
        .def("setParameterValue", &WarpVisualizationWidget::setParameterValue, "")
        .def("setPointCount", &WarpVisualizationWidget::setPointCount, "")
        .def("isDrawingHistogram", &WarpVisualizationWidget::isDrawingHistogram, "")
        .def("setDrawHistogram", &WarpVisualizationWidget::setDrawHistogram, "")
        .def("isDrawingGrid", &WarpVisualizationWidget::isDrawingGrid, "")
        .def("setDrawGrid", &WarpVisualizationWidget::setDrawGrid, "")
        .def_readwrite("window", &WarpVisualizationWidget::window);
}
