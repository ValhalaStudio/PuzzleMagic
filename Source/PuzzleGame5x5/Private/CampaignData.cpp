#include "CampaignData.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	bool ParseDir(const FString& Text, EPuzzleDir& Out)
	{
		if (Text.Equals(TEXT("up"), ESearchCase::IgnoreCase))    { Out = EPuzzleDir::Up; return true; }
		if (Text.Equals(TEXT("right"), ESearchCase::IgnoreCase)) { Out = EPuzzleDir::Right; return true; }
		if (Text.Equals(TEXT("down"), ESearchCase::IgnoreCase))  { Out = EPuzzleDir::Down; return true; }
		if (Text.Equals(TEXT("left"), ESearchCase::IgnoreCase))  { Out = EPuzzleDir::Left; return true; }
		return false;
	}

	bool ParseColor(const FString& Text, EPuzzleTileColor& Out)
	{
		if (Text.Equals(TEXT("black"), ESearchCase::IgnoreCase))  { Out = EPuzzleTileColor::Black; return true; }
		if (Text.Equals(TEXT("purple"), ESearchCase::IgnoreCase)) { Out = EPuzzleTileColor::Purple; return true; }
		if (Text.Equals(TEXT("red"), ESearchCase::IgnoreCase))    { Out = EPuzzleTileColor::Red; return true; }
		if (Text.Equals(TEXT("ash"), ESearchCase::IgnoreCase))    { Out = EPuzzleTileColor::Ash; return true; }
		return false;
	}

	FString GetString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
	{
		FString Value;
		Object->TryGetStringField(Key, Value);
		return Value;
	}

	int32 GetInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32 Default)
	{
		double Value = 0.0;
		return Object->TryGetNumberField(Key, Value) ? FMath::RoundToInt(static_cast<float>(Value)) : Default;
	}

	bool GetBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
	{
		bool bValue = false;
		Object->TryGetBoolField(Key, bValue);
		return bValue;
	}

	// {"cells": [[x, y], ...], "dirs": ["right", ...], "color": "purple"}: cells are shifted so the piece starts at 0, 0.
	bool ParsePiece(const TSharedPtr<FJsonObject>& Object, FPuzzlePieceShape& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
		if (!Object->TryGetArrayField(TEXT("cells"), Cells) || Cells->Num() < 1 || Cells->Num() > 4)
		{
			return false;
		}
		TArray<FIntPoint> Points;
		for (const TSharedPtr<FJsonValue>& CellValue : *Cells)
		{
			const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
			if (!CellValue->TryGetArray(Pair) || Pair->Num() != 2)
			{
				return false;
			}
			Points.Add(FIntPoint(FMath::RoundToInt(static_cast<float>((*Pair)[0]->AsNumber())), FMath::RoundToInt(static_cast<float>((*Pair)[1]->AsNumber()))));
		}
		FIntPoint Min = Points[0];
		for (const FIntPoint& Point : Points)
		{
			Min.X = FMath::Min(Min.X, Point.X);
			Min.Y = FMath::Min(Min.Y, Point.Y);
		}
		Out.Cells.Reset();
		for (const FIntPoint& Point : Points)
		{
			Out.Cells.Add(Point - Min);
		}

		Out.Dirs.Init(EPuzzleDir::Up, Out.Cells.Num());
		const TArray<TSharedPtr<FJsonValue>>* Dirs = nullptr;
		if (Object->TryGetArrayField(TEXT("dirs"), Dirs))
		{
			for (int32 I = 0; I < Dirs->Num() && I < Out.Dirs.Num(); ++I)
			{
				ParseDir((*Dirs)[I]->AsString(), Out.Dirs[I]);
			}
		}
		Out.Color = EPuzzleTileColor::Red;
		ParseColor(GetString(Object, TEXT("color")), Out.Color);
		return true;
	}

	// [[piece, ...], ...]: buckets of up to three pieces; empty buckets are dropped (they would leave the tray bare).
	void ParseBuckets(const TSharedPtr<FJsonObject>& Tray, const TCHAR* Key, TArray<TArray<FPuzzlePieceShape>>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Buckets = nullptr;
		if (!Tray->TryGetArrayField(Key, Buckets))
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& BucketValue : *Buckets)
		{
			const TArray<TSharedPtr<FJsonValue>>* Pieces = nullptr;
			if (!BucketValue->TryGetArray(Pieces))
			{
				continue;
			}
			TArray<FPuzzlePieceShape> Bucket;
			for (const TSharedPtr<FJsonValue>& PieceValue : *Pieces)
			{
				const TSharedPtr<FJsonObject>* PieceObject = nullptr;
				FPuzzlePieceShape Piece;
				if (PieceValue->TryGetObject(PieceObject) && ParsePiece(*PieceObject, Piece) && Bucket.Num() < 3)
				{
					Bucket.Add(Piece);
				}
			}
			if (Bucket.Num() > 0)
			{
				Out.Add(MoveTemp(Bucket));
			}
		}
	}

	void ParseStep(const TSharedPtr<FJsonObject>& Object, FArcadeChallenge& Step)
	{
		Step.Title = GetString(Object, TEXT("title"));
		Step.TargetScore = FMath::Max(0, GetInt(Object, TEXT("goal"), 0));
		Step.GoalRoutes = FMath::Max(0, GetInt(Object, TEXT("goalRoutes"), 0));
		bool bHold = true;
		Object->TryGetBoolField(TEXT("holdSlot"), bHold);
		Step.bHoldSlot = bHold;
		Step.bClearBuckets = GetBool(Object, TEXT("goalBuckets")); // dropped below if there are no buckets to use up
		if (Step.TargetScore == 0 && Step.GoalRoutes == 0 && !Step.bClearBuckets)
		{
			Step.TargetScore = 1; // a step needs a goal
		}
		Step.TimeLimitSeconds = FMath::Max(0, GetInt(Object, TEXT("timeLimit"), 0));
		Step.IntroText = GetString(Object, TEXT("intro"));
		Step.OutroText = GetString(Object, TEXT("outro"));
		Step.HintText = GetString(Object, TEXT("hint"));

		const TArray<TSharedPtr<FJsonValue>>* Grid = nullptr;
		if (Object->TryGetArrayField(TEXT("grid"), Grid) && Grid->Num() == 2)
		{
			Step.GridWidth = FMath::Clamp(FMath::RoundToInt(static_cast<float>((*Grid)[0]->AsNumber())), 4, 8);
			Step.GridHeight = FMath::Clamp(FMath::RoundToInt(static_cast<float>((*Grid)[1]->AsNumber())), 4, 8);
		}

		const TSharedPtr<FJsonObject>* Relics = nullptr;
		if (Object->TryGetObjectField(TEXT("relics"), Relics))
		{
			Step.bHolyLight = GetBool(*Relics, TEXT("holyLight"));
			Step.bReroll = GetBool(*Relics, TEXT("reroll"));
			Step.HolyLightCharges = FMath::Clamp(GetInt(*Relics, TEXT("holyLightCharges"), 1), 0, 3);
			Step.RerollCharges = FMath::Clamp(GetInt(*Relics, TEXT("rerollCharges"), 1), 0, 3);
		}
		const TSharedPtr<FJsonObject>* Bonus = nullptr;
		if (Object->TryGetObjectField(TEXT("bonus"), Bonus))
		{
			Step.bPumpkin = GetBool(*Bonus, TEXT("pumpkin"));
			Step.bOutgoingBottle = GetBool(*Bonus, TEXT("fullPotion"));
			Step.bIncomingBottle = GetBool(*Bonus, TEXT("emptyPotion"));
		}

		const TArray<TSharedPtr<FJsonValue>>* Board = nullptr;
		if (Object->TryGetArrayField(TEXT("board"), Board))
		{
			for (const TSharedPtr<FJsonValue>& TileValue : *Board)
			{
				const TSharedPtr<FJsonObject>* TileObject = nullptr;
				if (!TileValue->TryGetObject(TileObject))
				{
					continue;
				}
				FArcadeTile Tile;
				Tile.X = GetInt(*TileObject, TEXT("x"), 0);
				Tile.Y = GetInt(*TileObject, TEXT("y"), 0);
				const FString BonusName = GetString(*TileObject, TEXT("bonus"));
				if (BonusName == TEXT("pumpkin"))          { Tile.Bonus = EPuzzleBonus::Basic; }
				else if (BonusName == TEXT("fullPotion"))  { Tile.Bonus = EPuzzleBonus::Outgoing; }
				else if (BonusName == TEXT("emptyPotion")) { Tile.Bonus = EPuzzleBonus::Incoming; }
				ParseColor(GetString(*TileObject, TEXT("color")), Tile.Color);
				ParseDir(GetString(*TileObject, TEXT("dir")), Tile.Dir);
				Step.Board.Add(Tile);
			}
		}

		const TSharedPtr<FJsonObject>* Tray = nullptr;
		if (Object->TryGetObjectField(TEXT("tray"), Tray))
		{
			const FString After = GetString(*Tray, TEXT("after"));
			Step.TrayAfter = After == TEXT("loop") ? ECampaignTrayAfter::Loop : (After == TEXT("end") ? ECampaignTrayAfter::End : ECampaignTrayAfter::Random);
			ParseBuckets(*Tray, TEXT("buckets"), Step.Buckets);
			ParseBuckets(*Tray, TEXT("rerollBuckets"), Step.RerollBuckets);
			Step.RerollAfter = GetString(*Tray, TEXT("rerollAfter")) == TEXT("loop") ? ECampaignTrayAfter::Loop : ECampaignTrayAfter::Random;
		}

		// "Use up every bucket" needs preset buckets and a tray that ends after the last one.
		if (Step.bClearBuckets && Step.Buckets.Num() == 0)
		{
			Step.bClearBuckets = false;
			if (Step.TargetScore == 0 && Step.GoalRoutes == 0)
			{
				Step.TargetScore = 1;
			}
		}
		if (Step.bClearBuckets)
		{
			Step.TrayAfter = ECampaignTrayAfter::End;
		}
	}
}

TSharedPtr<FArcadeCampaign> FArcadeCampaign::Parse(const FString& JsonText, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("not valid JSON");
		return nullptr;
	}
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!Root->TryGetArrayField(TEXT("steps"), Steps))
	{
		OutError = TEXT("no \"steps\" list");
		return nullptr;
	}

	TSharedPtr<FArcadeCampaign> Campaign = MakeShared<FArcadeCampaign>();
	Campaign->Kind = GetString(Root, TEXT("kind"));
	Campaign->CampaignName = GetString(Root, TEXT("name"));
	Campaign->FinaleText = GetString(Root, TEXT("finale"));
	for (const TSharedPtr<FJsonValue>& StepValue : *Steps)
	{
		const TSharedPtr<FJsonObject>* StepObject = nullptr;
		if (StepValue->TryGetObject(StepObject))
		{
			FArcadeChallenge Step;
			ParseStep(*StepObject, Step);
			Campaign->Challenges.Add(MoveTemp(Step));
		}
	}
	return Campaign;
}

TSharedPtr<FArcadeCampaign> FArcadeCampaign::Load(const FString& Kind)
{
	const FString FileName = Kind + TEXT(".json");
	const FString Candidates[] = { FPaths::ProjectDir() / TEXT("Campaigns") / FileName, FPaths::ProjectContentDir() / TEXT("Campaigns") / FileName };
	for (const FString& Path : Candidates)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			continue;
		}
		FString Error;
		TSharedPtr<FArcadeCampaign> Campaign = Parse(Text, Error);
		if (Campaign.IsValid())
		{
			Campaign->Kind = Kind;
			UE_LOG(LogTemp, Display, TEXT("Campaign %s: %d steps from %s"), *Kind, Campaign->Challenges.Num(), *Path);
			return Campaign;
		}
		UE_LOG(LogTemp, Warning, TEXT("Campaign file %s could not be read: %s"), *Path, *Error);
	}
	return nullptr;
}
