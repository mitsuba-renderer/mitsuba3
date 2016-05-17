#pragma once

#include <mitsuba/mitsuba.h>
#include <string>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(xml)

/// Load a Mitsuba scene from an XML file
extern MTS_EXPORT_CORE ref<Object> loadFile(const fs::path &path);

/// Load a Mitsuba scene from an XML string
extern MTS_EXPORT_CORE ref<Object> loadString(const std::string &string);

NAMESPACE_END(xml)
NAMESPACE_END(mitsuba)
