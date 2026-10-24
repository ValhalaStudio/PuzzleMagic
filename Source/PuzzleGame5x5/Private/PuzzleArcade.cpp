// The campaign side of APuzzleGameMode: the arcade (challenges of escalating difficulty) and the tutorial (a step-by-step
// lesson), both read from JSON files made with the standalone campaign editor. A step sets the board size, the relics and
// the bonus tiles, can put tiles on the board and deal the player a fixed set of pieces, shows an intro popup, runs the
// clock, and ends with an outro popup when its goals are met, or a fail popup when time runs out or no piece fits.
#include "PuzzleGameMode.h"
#include "PuzzleDemo.h"
#include "PuzzleLocale.h"
#include "CampaignData.h"
#include "GridManager.h"
#include "PuzzleManager.h"
#include "PuzzleHUDWidget.h"
#include "PuzzleInputHandler.h"
#include "PuzzleSaveGame.h"

namespace
{
	const TCHAR* KindFileName[2] = { TEXT("arcade"), TEXT("tutorial") };
}

void APuzzleGameMode::PuzzleLang(const FString& Language)
{
	PuzzleLocale::SetOverride(Language);
	Campaigns[1].Reset(); // the tutorial is loaded again in the new language
	LoadCampaign();
	ShowMenu();           // back to the menu, so the next how-to page or tutorial step is built in the new language
	UE_LOG(LogTemp, Display, TEXT("PuzzleLang: Greek texts %s"), PuzzleLocale::IsGreek() ? TEXT("on") : TEXT("off"));
}

void APuzzleGameMode::LoadCampaign()
{
	for (int32 Kind = 0; Kind < 2; ++Kind)
	{
		if (!Campaigns[Kind].IsValid())
		{
			// The tutorial has a Greek copy (tutorial_el.json); with Greek off, or without the file, it is the English one.
			if (Kind == 1 && PuzzleLocale::IsGreek())
			{
				Campaigns[Kind] = FArcadeCampaign::Load(TEXT("tutorial_el"));
			}
			if (!Campaigns[Kind].IsValid())
			{
				Campaigns[Kind] = FArcadeCampaign::Load(KindFileName[Kind]);
			}
		}
	}
}

bool APuzzleGameMode::HasCampaign(int32 Kind) const
{
	return Kind >= 0 && Kind < 2 && Campaigns[Kind].IsValid() && Campaigns[Kind]->Challenges.Num() > 0;
}

int32 APuzzleGameMode::GetArcadeCount() const
{
	const FArcadeCampaign* Campaign = GetArcadeCampaign();
	return Campaign ? Campaign->Challenges.Num() : 0;
}

const FArcadeChallenge* APuzzleGameMode::GetArcadeChallenge() const
{
	const FArcadeCampaign* Campaign = GetArcadeCampaign();
	return (Campaign && Campaign->Challenges.IsValidIndex(ArcadeIndex)) ? &Campaign->Challenges[ArcadeIndex] : nullptr;
}

int32 APuzzleGameMode::GetArcadeProgress() const
{
	if (!SaveGame)
	{
		return 0;
	}
	return FMath::Clamp(ActiveKind == 0 ? SaveGame->ArcadeProgress : SaveGame->TutorialProgress, 0, GetArcadeCount());
}

void APuzzleGameMode::OpenCampaign(int32 Kind)
{
	if (!HasCampaign(Kind))
	{
		return;
	}
	ActiveKind = Kind;
	// With nothing beaten yet it goes straight to the first step; otherwise it offers to continue.
	if (GetArcadeProgress() == 0)
	{
		StartArcade(0);
	}
	else if (UPuzzleHUDWidget* UI = GetUI())
	{
		UI->ShowCard(EPuzzleCard::ArcadeMenu);
	}
}

void APuzzleGameMode::StartArcade(int32 FromIndex)
{
	if (!HasCampaign(ActiveKind) || !PuzzleManager || !GridManager)
	{
		return;
	}
	SetAutoPlay(false);
	bArcade = true;
	ArcadeIndex = FMath::Clamp(FromIndex, 0, GetArcadeCount() - 1);
	PrepareArcadeChallenge(true);
}

void APuzzleGameMode::PrepareArcadeChallenge(bool bShowIntro)
{
	const FArcadeChallenge* Challenge = GetArcadeChallenge();
	if (!Challenge || !PuzzleManager || !GridManager)
	{
		return;
	}
	PuzzleManager->ConfigureItems(Challenge->bHolyLight, Challenge->bReroll, Challenge->bPumpkin, Challenge->bOutgoingBottle, Challenge->bIncomingBottle, Challenge->HolyLightCharges, Challenge->RerollCharges);
	GridManager->bHoldEnabled = Challenge->bHoldSlot;
	GridManager->SetGridSize(Challenge->GridWidth, Challenge->GridHeight); // also rebuilds the tray panels
	ReframeCamera();
	// A fixed tray (or none: random pieces), then the board with its starting tiles.
	GridManager->SetCuratedTray(Challenge->Buckets, Challenge->TrayAfter, Challenge->RerollBuckets, Challenge->RerollAfter);
	StartRound();
	for (const FArcadeTile& Tile : Challenge->Board)
	{
		if (Tile.Bonus != EPuzzleBonus::None)
		{
			GridManager->PlaceFixedBonus(Tile.X, Tile.Y, Tile.Bonus);
		}
		else
		{
			GridManager->PlaceFixedTile(Tile.X, Tile.Y, Tile.Color, Tile.Dir);
		}
	}

	ArcadeTimeLeft = static_cast<float>(Challenge->TimeLimitSeconds);
	ArcadeFailReason.Reset();
	if (bShowIntro)
	{
		// The board waits behind the popup; the clock starts with the START button.
		ArcadeState = EArcadeState::Intro;
		Flow = EPuzzleFlow::Menu;
		if (UPuzzleHUDWidget* UI = GetUI())
		{
			UI->ShowCard(EPuzzleCard::ArcadeIntro);
		}
	}
	else
	{
		BeginArcadeChallenge();
	}
}

void APuzzleGameMode::BeginArcadeChallenge()
{
	const FArcadeChallenge* Challenge = GetArcadeChallenge();
	if (!bArcade || !Challenge)
	{
		return;
	}
	ArcadeState = EArcadeState::Playing;
	ArcadeTimeLeft = static_cast<float>(Challenge->TimeLimitSeconds);
	bGameOver = false;
	Flow = EPuzzleFlow::Playing;
	if (UPuzzleHUDWidget* UI = GetUI())
	{
		UI->ShowCard(EPuzzleCard::None);
	}
}

void APuzzleGameMode::ArcadeRetry()
{
	if (bArcade)
	{
		PrepareArcadeChallenge(false);
	}
}

void APuzzleGameMode::ArcadeAdvance()
{
	if (!bArcade)
	{
		return;
	}
	++ArcadeIndex;
	if (ArcadeIndex >= GetArcadeCount())
	{
		ArcadeIndex = GetArcadeCount() - 1;
		ArcadeState = EArcadeState::Done;
		Flow = EPuzzleFlow::Menu;
		if (UPuzzleHUDWidget* UI = GetUI())
		{
			UI->ShowCard(EPuzzleCard::ArcadeDone);
		}
		return;
	}
	PrepareArcadeChallenge(true);
}

void APuzzleGameMode::LeaveArcade()
{
	if (!bArcade)
	{
		return;
	}
	bArcade = false;
	ArcadeState = EArcadeState::None;
	if (GridManager)
	{
		GridManager->ClearCuratedTray();
	}
	// Back to the options and board size the player picked in the menus.
	if (SaveGame && PuzzleManager)
	{
		PuzzleManager->SetExtrasEnabled(SaveGame->bOptionRelics);
		PuzzleManager->SetBonusTilesEnabled(SaveGame->bOptionBonusTiles);
		if (GridManager)
		{
			GridManager->bHoldEnabled = true;
			GridManager->SetGridSize(SaveGame->CourseWidth, SaveGame->CourseHeight);
			ReframeCamera();
		}
		if (PuzzleDemo::IsDemo())
		{
			// The demo has no course or option choice: back to the board and the options of the demo mode being played.
			PuzzleManager->SetExtrasEnabled(true);
			PuzzleManager->SetBonusTilesEnabled(PuzzleDemo::BonusTiles());
			if (GridManager)
			{
				GridManager->SetGridSize(PuzzleDemo::BoardSize(), PuzzleDemo::BoardSize());
				ReframeCamera();
			}
		}
		StartRound();
	}
}

void APuzzleGameMode::CheckArcadeWin()
{
	const FArcadeChallenge* Challenge = GetArcadeChallenge();
	if (!bArcade || ArcadeState != EArcadeState::Playing || !Challenge || !PuzzleManager)
	{
		return;
	}
	// Every goal that is set must be met.
	if ((Challenge->TargetScore > 0 && PuzzleManager->Score < Challenge->TargetScore)
		|| (Challenge->GoalRoutes > 0 && PuzzleManager->RoutesCleared < Challenge->GoalRoutes)
		|| (Challenge->bClearBuckets && !(GridManager && GridManager->IsOutOfCuratedPieces())))
	{
		return;
	}
	ArcadeState = EArcadeState::Won;
	bGameOver = true; // no more moves while the last clear plays out
	if (InputHandler)
	{
		InputHandler->CancelInteraction();
	}
	if (SaveGame)
	{
		int32& Progress = ActiveKind == 0 ? SaveGame->ArcadeProgress : SaveGame->TutorialProgress;
		if (Progress < ArcadeIndex + 1)
		{
			Progress = ArcadeIndex + 1;
			SaveGame->Save();
		}
	}
	Later(AGridManager::ArriveDuration + 1.6f, [this]()
	{
		if (!bArcade || ArcadeState != EArcadeState::Won)
		{
			return;
		}
		Flow = EPuzzleFlow::Menu;
		if (UPuzzleHUDWidget* UI = GetUI())
		{
			UI->ShowCard(EPuzzleCard::ArcadeOutro);
		}
	});
}

void APuzzleGameMode::FailArcade(const FString& Reason)
{
	if (!bArcade || ArcadeState != EArcadeState::Playing)
	{
		return;
	}
	ArcadeState = EArcadeState::Failed;
	ArcadeFailReason = Reason;
	bGameOver = true;
	if (InputHandler)
	{
		InputHandler->CancelInteraction();
	}
	Later(1.0f, [this]()
	{
		if (!bArcade || ArcadeState != EArcadeState::Failed)
		{
			return;
		}
		Flow = EPuzzleFlow::Menu;
		if (UPuzzleHUDWidget* UI = GetUI())
		{
			UI->ShowCard(EPuzzleCard::ArcadeFail);
		}
	});
}

void APuzzleGameMode::PuzzleArcadeTest(const FString& What)
{
	const FArcadeChallenge* Challenge = GetArcadeChallenge();
	if (!bArcade || !Challenge || !PuzzleManager)
	{
		return;
	}
	if (What == TEXT("win"))
	{
		PuzzleManager->Score = FMath::Max(PuzzleManager->Score, Challenge->TargetScore);
		PuzzleManager->RoutesCleared = FMath::Max(PuzzleManager->RoutesCleared, Challenge->GoalRoutes);
		CheckArcadeWin();
	}
	else if (What == TEXT("fail"))
	{
		FailArcade(TEXT("Test failure"));
	}
	else if (What == TEXT("time"))
	{
		ArcadeTimeLeft = 5.f;
	}
}

void APuzzleGameMode::PuzzleUiCard(const FString& What, int32 A, int32 B, int32 Nonce)
{
	UPuzzleHUDWidget* UI = GetUI();
	if (!UI)
	{
		return;
	}
	if (What == TEXT("menu"))
	{
		ShowMenu();
	}
	else if (What == TEXT("game"))
	{
		ShowMenu();
		StartEndless();
	}
	else if (What == TEXT("courses"))   { UI->ShowCard(EPuzzleCard::Courses); }
	else if (What == TEXT("options"))   { UI->ShowCard(EPuzzleCard::Options); }
	else if (What == TEXT("pause"))     { UI->ShowCard(EPuzzleCard::Pause); }
	else if (What == TEXT("theme"))     { SetTheme(PuzzleStyle::NextTheme(GetTheme())); } // what the options button does: next theme, saved, map reloaded
	else if (What == TEXT("demo"))      { StartDemoGame(A); } // what the menu's DEMO 1 / DEMO 2 buttons do: PuzzleUiCard demo 1|2
	else if (What == TEXT("demowin"))   { if (PuzzleDemo::IsDemo() && PuzzleManager) { PuzzleManager->Score = PuzzleDemo::ScoreCap(); CheckDemoCap(); } } // test: reach the demo cap
	else if (What == TEXT("gameover"))  { UI->ShowCard(EPuzzleCard::GameOver); }
	else if (What == TEXT("howto"))     { UI->ShowTutorial(A); }
	else if (What == TEXT("arcademenu") && HasCampaign(A))
	{
		ActiveKind = A;
		UI->ShowCard(EPuzzleCard::ArcadeMenu);
	}
	else if (What == TEXT("arcadeintro") && HasCampaign(A))
	{
		ActiveKind = A;
		StartArcade(B); // shows the intro popup of step B
	}
	else if (What == TEXT("arcadebegin"))
	{
		BeginArcadeChallenge();
	}
	else if (What == TEXT("arcadewin") || What == TEXT("arcadefail") || What == TEXT("arcadetime"))
	{
		PuzzleArcadeTest(What.RightChop(6)); // win, fail or time
	}
	else if (What == TEXT("arcadedone") && HasCampaign(A))
	{
		ActiveKind = A;
		bArcade = true;
		ArcadeIndex = GetArcadeCount() - 1;
		ArcadeState = EArcadeState::Done;
		Flow = EPuzzleFlow::Menu;
		UI->ShowCard(EPuzzleCard::ArcadeDone);
	}
}

void APuzzleGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if !(PLATFORM_ANDROID || PLATFORM_IOS)
	// The window mode can also change from outside the options card (Alt+Enter, F11): keep the choice saved and the card honest.
	const int32 WindowMode = IsFullscreen() ? 1 : 0;
	if (WindowMode != LastWindowMode)
	{
		if (LastWindowMode >= 0)
		{
			if (SaveGame && SaveGame->DisplayMode != WindowMode)
			{
				SaveGame->DisplayMode = WindowMode;
				SaveGame->Save();
			}
			if (UPuzzleHUDWidget* UI = GetUI())
			{
				UI->OnWindowModeChanged();
			}
		}
		LastWindowMode = WindowMode;
	}
#endif
	const FArcadeChallenge* Challenge = GetArcadeChallenge();
	if (bArcade && ArcadeState == EArcadeState::Playing && Challenge && Challenge->TimeLimitSeconds > 0 && Flow == EPuzzleFlow::Playing && !bPauseMenuOpen && !bGameOver)
	{
		// A long hitch (the app in the background) must not eat the player's time.
		ArcadeTimeLeft -= FMath::Min(DeltaSeconds, 0.25f);
		if (ArcadeTimeLeft <= 0.f)
		{
			ArcadeTimeLeft = 0.f;
			FailArcade(TEXT("Time's up!"));
		}
	}
}
