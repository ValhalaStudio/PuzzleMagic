#pragma once

// Defines DLLEXPORT/DLLIMPORT, which UBT's MESHOPTIMIZER_API expands to.
#include "HAL/Platform.h"

// Third-party code: UE promotes MSVC's implicit-conversion-to-bool warning to an error.
#ifdef _MSC_VER
#pragma warning(disable : 4800)
#endif
