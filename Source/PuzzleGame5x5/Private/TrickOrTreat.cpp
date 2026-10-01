#include "TrickOrTreat.h"
#include "PuzzleManager.h"
#include "GridManager.h"
#include "PieceLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UTrickOrTreat::Init(UPuzzleManager* InRules, AGridManager* InBoard)
{
	Rules = InRules;
	Board = InBoard;
	Store = MakeShared<FTestPuzzleStore>();
	// -tricksegment=N: the wheel always stops on segment N (0-7), so a recording shows a chosen fortune.
	FParse::Value(FCommandLine::Get(), TEXT("tricksegment="), ForcedSegment);
}

void UTrickOrTreat::StartRound()
{
	NextPacketScore = PacketEvery;
	bAvailable = false;
}

bool UTrickOrTreat::CheckArrival()
{
	if (!Rules || !Rules->bRelicsEnabled || bAvailable || Rules->Score < NextPacketScore)
	{
		return false;
	}
	NextPacketScore = (Rules->Score / PacketEvery + 1) * PacketEvery;
	bAvailable = true;
	return true;
}

const TArray<FTreatOffer>& UTrickOrTreat::Offers()
{
	auto Make = [](const TCHAR* Id, const TCHAR* Title, const TCHAR* Detail, const TCHAR* Price, bool bBundle,
		int32 Luck, int32 HolyLights, int32 Rerolls, int32 Tiles, int32 Pumpkins)
	{
		FTreatOffer Offer;
		Offer.ProductId = Id;
		Offer.Title = Title;
		Offer.Detail = Detail;
		Offer.Price = Price;
		Offer.bBundle = bBundle;
		Offer.Luck = Luck;
		Offer.HolyLights = HolyLights;
		Offer.Rerolls = Rerolls;
		Offer.Tiles = Tiles;
		Offer.Pumpkins = Pumpkins;
		return Offer;
	};
	// Singles first, then the bundles (each cheaper than its parts bought one by one).
	static const TArray<FTreatOffer> List = {
		Make(TEXT("treat.lucky_candle"), TEXT("Lucky Candle"), TEXT("+25 luck"), TEXT("$0.99"), false, LuckyCandleLuck, 0, 0, 0, 0),
		Make(TEXT("treat.sugar_skull"), TEXT("Sugar Skull"), TEXT("an extra tile in HOLD"), TEXT("$0.99"), false, 0, 0, 0, 1, 0),
		Make(TEXT("treat.holy_light"), TEXT("Holy Light"), TEXT("+1 Holy Light relic"), TEXT("$1.99"), false, 0, 1, 0, 0, 0),
		Make(TEXT("bundle.witches_brew"), TEXT("Witch's Brew"), TEXT("+25 luck, Holy Light, Reroll"), TEXT("$2.99"), true, 25, 1, 1, 0, 0),
		Make(TEXT("bundle.pumpkin_patch"), TEXT("Pumpkin Patch"), TEXT("2 pumpkins, Sugar Skull, +15 luck"), TEXT("$3.49"), true, 15, 0, 0, 1, 2),
		Make(TEXT("bundle.haunted_hoard"), TEXT("Haunted Hoard"), TEXT("+50 luck, 2 Holy Lights, Reroll, Sugar Skull"), TEXT("$4.99"), true, 50, 2, 1, 1, 0),
	};
	return List;
}

void UTrickOrTreat::BuyTreat(int32 Index, TFunction<void(bool bSuccess)> OnDone)
{
	if (!Rules || !Board || !Store || !Offers().IsValidIndex(Index))
	{
		OnDone(false);
		return;
	}
	TWeakObjectPtr<UTrickOrTreat> WeakThis(this);
	Store->Purchase(Offers()[Index].ProductId, [WeakThis, Index, OnDone](bool bSuccess)
	{
		UTrickOrTreat* Self = WeakThis.Get();
		if (!Self || !bSuccess || !Self->Rules || !Self->Board)
		{
			OnDone(false);
			return;
		}
		const FTreatOffer& Offer = Offers()[Index];
		Self->Rules->GrantLuck(Offer.Luck);
		for (int32 I = 0; I < Offer.HolyLights; ++I)
		{
			Self->Rules->GrantRelic(ERelic::HolyLight);
		}
		for (int32 I = 0; I < Offer.Rerolls; ++I)
		{
			Self->Rules->GrantRelic(ERelic::Reroll);
		}
		if (Offer.Tiles > 0)
		{
			// One tile, any hand: the most placeable piece there is.
			FPuzzlePieceShape Skull = PieceLibrary::MakeRandomPieceRandomColor();
			Skull.Cells = { FIntPoint(0, 0) };
			Skull.Dirs = { static_cast<EPuzzleDir>(FMath::RandRange(0, 3)) };
			Self->Board->GiveReservePiece(Skull);
		}
		for (int32 I = 0; I < Offer.Pumpkins; ++I)
		{
			Self->Rules->SummonPumpkin();
		}
		OnDone(true);
	});
}

int32 UTrickOrTreat::SpinTrick()
{
	return ForcedSegment >= 0 && ForcedSegment < SegmentCount ? ForcedSegment : FMath::RandRange(0, SegmentCount - 1);
}

FString UTrickOrTreat::SegmentLabel(int32 Segment)
{
	static const TCHAR* Labels[SegmentCount] = {
		TEXT("+15 LUCK"), TEXT("-15 LUCK"), TEXT("REROLL"), TEXT("STRAY TILE"),
		TEXT("x2 NEXT"), TEXT("HALF NEXT"), TEXT("PUMPKIN"), TEXT("COMBO LOST") };
	return Labels[FMath::Clamp(Segment, 0, SegmentCount - 1)];
}

FString UTrickOrTreat::ApplyTrick(int32 Segment)
{
	if (!Rules)
	{
		return FString();
	}
	switch (Segment)
	{
	case 0:
		Rules->GrantLuck(TrickLuck);
		return TEXT("The moon smiles: +15 luck");
	case 1:
		Rules->GrantLuck(-TrickLuck);
		return TEXT("A black cat crosses: -15 luck");
	case 2:
		Rules->FreeReroll();
		return TEXT("A fresh hand, free of charge");
	case 3:
		if (Rules->DropStrayTile())
		{
			return TEXT("Something left a tile on your board");
		}
		Rules->GrantLuck(-TrickLuck);
		return TEXT("No room for mischief: -15 luck");
	case 4:
		Rules->NextClearMultiplier = 2.f;
		return TEXT("Your next clear scores double");
	case 5:
		Rules->NextClearMultiplier = 0.5f;
		return TEXT("Your next clear scores half");
	case 6:
		if (Rules->SummonPumpkin())
		{
			return TEXT("A pumpkin rolls onto the board");
		}
		Rules->GrantLuck(TrickLuck);
		return TEXT("No room for a pumpkin: +15 luck");
	default:
		Rules->BreakCombo();
		return TEXT("The candles gutter: combo lost");
	}
}
