#pragma once

#include <mitsuba/mitsuba.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(xml)

extern MTS_EXPORT_CORE ref<Object> load(const fs::path &path);

NAMESPACE_END(xml)
NAMESPACE_END(mitsuba)
