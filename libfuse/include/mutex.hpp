#pragma once

#ifdef DEBUG
#include "mutex_debug.hpp"
#else
#include "mutex_ndebug.hpp"
#endif
