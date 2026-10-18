#pragma once

#include "CoreMinimal.h"

namespace PuzzleStyle
{
	// The Classic look is the Route puzzle MVP as it was before the Halloween theme: a triangle on every tile (not a skeleton
	// hand), the older tile colours, a diamond / fisheye / plain tile for the bonus tiles (not potions and a pumpkin), plain
	// clears, and no cauldrons, bats or route streams. A test option (Play options); set by the game mode from the save file
	// before the scenery is built, and changed by reloading the map.
	inline bool GClassic = false;
	inline bool IsClassic() { return GClassic; }
	inline void SetClassic(bool bClassic) { GClassic = bClassic; }
}
