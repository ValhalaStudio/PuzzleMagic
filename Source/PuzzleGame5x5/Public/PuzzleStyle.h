#pragma once

#include "CoreMinimal.h"

// Themes: the look of the board and its surroundings. Adding a theme is one entry in PuzzleStyle::Defs() (and its scenery, if it
// brings its own), plus an entry in EPuzzleTheme; the save file, the options button, the -theme= launch flag and the lightweight
// mode all go through the table.
//   Desk      the default: plain triangle tiles on a quiet wooden desk.
//   Halloween the skeleton-hand tiles, potions and pumpkin, in the gothic cathedral with cauldrons, bats and route streams.
//   Classic   the Route puzzle MVP as it was before the Halloween theme: triangle tiles in the cathedral, without the props.
enum class EPuzzleTheme : uint8
{
	Desk = 0,
	Halloween = 1,
	Classic = 2,
	Count
};

enum class EPuzzleScenery : uint8
{
	Cathedral, // AGothicEnvironment (and, in the lightweight mode, its pre-rendered picture)
	Desk       // ADeskEnvironment: real geometry, cheap enough for every device
};

struct FPuzzleThemeDef
{
	const TCHAR* Id;            // -theme=<Id> on the command line, and what the save file keeps
	const TCHAR* Name;          // the options button
	const TCHAR* RouteMaterial; // the route tile material
	const TCHAR* BonusMaterial; // the bonus tile material
	bool bHandTiles;            // skeleton hands, potions and a pumpkin; otherwise triangles with the plain bonus symbols
	bool bProps;                // cauldrons, bats, steam, route streams and the bonus clear animation
	bool bPlainBoard;           // a plain graphite board, tray and cells instead of veined marble
	bool bInkOutline;           // the cartoon ink lines (a post-process on depth and normal breaks); off for a plain, flat look
	EPuzzleScenery Scenery;
	const TCHAR* LiteBackdrop;  // the pre-rendered picture of a Cathedral scene in the lightweight mode (null for the others)
};

namespace PuzzleStyle
{
	inline const FPuzzleThemeDef& Defs(EPuzzleTheme Theme)
	{
		static const FPuzzleThemeDef Table[] = {
			{ TEXT("desk"), TEXT("DESK"),
			  TEXT("/Game/Materials/M_TileRouteClassic.M_TileRouteClassic"), TEXT("/Game/Materials/M_TileBonusClassic.M_TileBonusClassic"),
			  false, false, true, false, EPuzzleScenery::Desk, nullptr },
			{ TEXT("halloween"), TEXT("HALLOWEEN"),
			  TEXT("/Game/Materials/M_TileRoute.M_TileRoute"), TEXT("/Game/Materials/M_TileBonus.M_TileBonus"),
			  true, true, false, true, EPuzzleScenery::Cathedral, TEXT("/Game/Lite/T_LiteBackdrop.T_LiteBackdrop") },
			{ TEXT("classic"), TEXT("CLASSIC"),
			  TEXT("/Game/Materials/M_TileRouteClassic.M_TileRouteClassic"), TEXT("/Game/Materials/M_TileBonusClassic.M_TileBonusClassic"),
			  false, false, false, true, EPuzzleScenery::Cathedral, TEXT("/Game/Lite/T_LiteBackdropClassic.T_LiteBackdropClassic") },
		};
		static_assert(UE_ARRAY_COUNT(Table) == static_cast<int32>(EPuzzleTheme::Count), "one theme definition per EPuzzleTheme");
		const int32 Index = static_cast<int32>(Theme);
		return Table[Index >= 0 && Index < UE_ARRAY_COUNT(Table) ? Index : 0];
	}

	// Set by the game mode from the save file (or -theme=) before the scenery is built; changing it reloads the map.
	inline EPuzzleTheme GTheme = EPuzzleTheme::Desk;
	inline EPuzzleTheme GetTheme() { return GTheme; }
	inline void SetTheme(EPuzzleTheme Theme) { GTheme = Theme; }
	inline const FPuzzleThemeDef& Current() { return Defs(GTheme); }

	// The theme after this one, for the options button.
	inline EPuzzleTheme NextTheme(EPuzzleTheme Theme)
	{
		return static_cast<EPuzzleTheme>((static_cast<int32>(Theme) + 1) % static_cast<int32>(EPuzzleTheme::Count));
	}

	// The theme with this id ("desk", "halloween", "classic"); false when there is none.
	inline bool FindTheme(const FString& Id, EPuzzleTheme& OutTheme)
	{
		for (int32 I = 0; I < static_cast<int32>(EPuzzleTheme::Count); ++I)
		{
			if (Id.Equals(Defs(static_cast<EPuzzleTheme>(I)).Id, ESearchCase::IgnoreCase))
			{
				OutTheme = static_cast<EPuzzleTheme>(I);
				return true;
			}
		}
		return false;
	}

	inline bool HandTiles() { return Current().bHandTiles; }
	inline bool HasProps() { return Current().bProps; }
	inline bool PlainBoard() { return Current().bPlainBoard; }
	inline bool InkOutline() { return Current().bInkOutline; }
	inline bool HasCathedral() { return Current().Scenery == EPuzzleScenery::Cathedral; }
}
