#pragma once

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if PLATFORM_ANDROID
#include <sys/system_properties.h>
#endif

namespace PuzzleLite
{
	// Phones and tablets get a lightweight scene: a pre-rendered backdrop picture and flat 2D sprites in place of the
	// real-time 3D scenery. A PC can try it with -lite; on Android `adb shell setprop debug.puzzle.lite 0` switches
	// it off again (for comparing frame rates).
	// The Graphics option (High / Low) of the options card: on a PC "Low" switches the same lightweight scene on. Set by the
	// game mode from the save file before the scenery is built.
	inline bool GLowGraphics = false;
	inline void SetLowGraphics(bool bLow) { GLowGraphics = bLow; }
	// Phones and tablets are always lightweight: no High option there.
	inline bool IsMobile()
	{
#if PLATFORM_ANDROID || PLATFORM_IOS
		return true;
#else
		return false;
#endif
	}

	inline bool IsLite()
	{
		static const bool bPlatformLite = []() -> bool
		{
			if (FParse::Param(FCommandLine::Get(), TEXT("lite")))
			{
				return true;
			}
#if PLATFORM_ANDROID
			char Value[PROP_VALUE_MAX] = {};
			if (__system_property_get("debug.puzzle.lite", Value) > 0 && Value[0] == '0')
			{
				return false;
			}
			return true;
#elif PLATFORM_IOS
			return true;
#else
			return false;
#endif
		}();
		return bPlatformLite || GLowGraphics;
	}
}
