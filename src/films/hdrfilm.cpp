#include <mitsuba/render/film.h>

NAMESPACE_BEGIN(mitsuba)

// TODO: documentation
class HDRFilm : public Film {
public:
    HDRFilm(const Properties &props) : Film(props) {
        configure();
    }

    virtual std::string to_string() const override {
        std::ostringstream oss;
        oss << "HDRFilm[not implemented]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()

private:
    void configure() { }
};

MTS_IMPLEMENT_CLASS(HDRFilm, Film);
MTS_EXPORT_PLUGIN(HDRFilm, "HDR Film");
NAMESPACE_END(mitsuba)
