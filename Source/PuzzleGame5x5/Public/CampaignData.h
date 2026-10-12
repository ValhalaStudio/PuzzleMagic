#pragma once

#include "CoreMinimal.h"
#include "PuzzleTypes.h"

// Where a campaign step (a challenge of the arcade, a step of the tutorial) stands.
enum class EArcadeState : uint8
{
	None,
	Intro,    // the "before" popup is up
	Playing,  // the clock is running
	Won,      // the goal was reached; the "after" popup follows
	Failed,   // time ran out, or no piece fits
	Done      // the last step is beaten
};

// A tile that is on the board when a step starts: a normal tile (colour and direction) or a bonus tile.
struct FArcadeTile
{
	int32 X = 0; // 0 is the left column
	int32 Y = 0; // 0 is the bottom row
	EPuzzleTileColor Color = EPuzzleTileColor::Red;
	EPuzzleDir Dir = EPuzzleDir::Up;
	EPuzzleBonus Bonus = EPuzzleBonus::None; // not None: a pumpkin or a potion bottle instead of a normal tile
};

// One step of a campaign: the arcade's challenge or the tutorial's lesson.
struct FArcadeChallenge
{
	FString Title = TEXT("New step");

	// The step is won when every goal that is set is met. At least one is set.
	int32 TargetScore = 0;  // reach this score (0: no score goal)
	int32 GoalRoutes = 0;   // clear this many routes (0: no route goal)
	bool bClearBuckets = false; // use up every preset bucket (the tray then ends after the last one)

	int32 TimeLimitSeconds = 0; // 0 is no time limit
	int32 GridWidth = 8;
	int32 GridHeight = 8;

	bool bHolyLight = false;
	bool bReroll = false;
	bool bPumpkin = false;
	bool bOutgoingBottle = false; // the full potion
	bool bIncomingBottle = false; // the empty potion
	bool bHoldSlot = true;        // false takes the hold slot away
	int32 HolyLightCharges = 1;   // charges the player starts with (0 to 3)
	int32 RerollCharges = 1;

	FString IntroText; // the popup before the step
	FString OutroText; // the popup after it
	FString HintText;  // shown on screen while playing

	// Tiles on the board at the start.
	TArray<FArcadeTile> Board;

	// The tray: when set, the player is dealt these buckets of pieces in order instead of random ones. A bucket is up
	// to three pieces (the tray's slots). After the last bucket the tray deals random pieces, starts over, or ends.
	TArray<TArray<FPuzzlePieceShape>> Buckets;
	ECampaignTrayAfter TrayAfter = ECampaignTrayAfter::Random;

	// The pieces each Reroll deals, in order (empty: random pieces). After the last one: start over (Loop) or random.
	TArray<TArray<FPuzzlePieceShape>> RerollBuckets;
	ECampaignTrayAfter RerollAfter = ECampaignTrayAfter::Random;
};

// A campaign: arcade.json (challenges of escalating difficulty) or tutorial.json (a step-by-step lesson), made with the
// standalone campaign editor (Tools/CampaignEditor/campaign_editor.html).
struct FArcadeCampaign
{
	FString Kind;                 // "arcade" or "tutorial"
	FString CampaignName;
	FString FinaleText;           // the popup after the last step
	TArray<FArcadeChallenge> Challenges;

	// Reads <Kind>.json: from <game>/Campaigns/ first (drop a file there to try it without rebuilding), then from the
	// bundled Content/Campaigns/. Null if there is no such file or it cannot be read.
	static TSharedPtr<FArcadeCampaign> Load(const FString& Kind);

	// Parses the JSON text. On failure returns null and says why in OutError.
	static TSharedPtr<FArcadeCampaign> Parse(const FString& JsonText, FString& OutError);
};
