#include "PuzzleGameMode.h"
#include "GridManager.h"
#include "PuzzleManager.h"
#include "PuzzleCameraPawn.h"
#include "PuzzleInputHandler.h"
#include "PuzzleBotPlayer.h"
#include "PuzzleHUD.h"
#include "PuzzleHUDWidget.h"
#include "PuzzleFX.h"
#include "PuzzleSaveGame.h"
#include "GothicEnvironment.h"
#include "HalloweenProps.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Components/AudioComponent.h"
#include "AudioMixerBlueprintLibrary.h"

APuzzleGameMode::APuzzleGameMode()
{
	GridManagerClass = AGridManager::StaticClass();
	InputHandlerClass = APuzzleInputHandler::StaticClass();
	DefaultPawnClass = APuzzleCameraPawn::StaticClass();
	HUDClass = APuzzleHUD::StaticClass();

	// Synthesized gothic set (organ, choir, church bells, music box, stone, thunder): see RawAudio/.
	static ConstructorHelpers::FObjectFinder<USoundBase> PlaceFinder(TEXT("/Game/Audio/SFXG_Place.SFXG_Place"));
	static ConstructorHelpers::FObjectFinder<USoundBase> ClearFinder(TEXT("/Game/Audio/SFXG_Clear.SFXG_Clear"));
	static ConstructorHelpers::FObjectFinder<USoundBase> GameOverFinder(TEXT("/Game/Audio/SFXG_GameOver.SFXG_GameOver"));
	static ConstructorHelpers::FObjectFinder<USoundBase> HolyFinder(TEXT("/Game/Audio/SFXG_Holy.SFXG_Holy"));
	static ConstructorHelpers::FObjectFinder<USoundBase> RelicFinder(TEXT("/Game/Audio/SFXG_Relic.SFXG_Relic"));
	static ConstructorHelpers::FObjectFinder<USoundBase> ComboLostFinder(TEXT("/Game/Audio/SFXG_ComboLost.SFXG_ComboLost"));
	static ConstructorHelpers::FObjectFinder<USoundBase> MusicFinder(TEXT("/Game/Audio/MUS_Chant.MUS_Chant"));
	static ConstructorHelpers::FObjectFinder<USoundBase> OrganFinder(TEXT("/Game/Audio/MUS_Gothic.MUS_Gothic"));
	OrganMusicSound = OrganFinder.Object;
	PlaceSound = PlaceFinder.Object;
	ClearSound = ClearFinder.Object;
	GameOverSound = GameOverFinder.Object;
	HolySound = HolyFinder.Object;
	RelicSound = RelicFinder.Object;
	ComboLostSound = ComboLostFinder.Object;
	MusicSound = MusicFinder.Object;
}

void APuzzleGameMode::StartAudioRecording(float Seconds)
{
	// The engine mutes an unfocused window (UnfocusedVolumeMultiplier = 0), which would silence the capture.
	FApp::SetUnfocusedVolumeMultiplier(1.f);
	UAudioMixerBlueprintLibrary::StartRecordingOutput(this, Seconds);
	UE_LOG(LogTemp, Display, TEXT("PuzzleRecord: audio recording started (%.1f s)"), Seconds);
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, TEXT("demo_audio"), FPaths::ProjectSavedDir() / TEXT("Recording"));
		UE_LOG(LogTemp, Display, TEXT("PuzzleRecord: audio saved"));
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
	}), Seconds, false);
}

void APuzzleGameMode::StartPlay()
{
	Super::StartPlay();

	SaveGame = UPuzzleSaveGame::LoadOrCreate();
	PuzzleManager = NewObject<UPuzzleManager>(this);

	if (GetWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		GridManager = GetWorld()->SpawnActor<AGridManager>(GridManagerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		InputHandler = GetWorld()->SpawnActor<APuzzleInputHandler>(InputHandlerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		Environment = GetWorld()->SpawnActor<AGothicEnvironment>(AGothicEnvironment::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		GetWorld()->SpawnActor<AHalloweenProps>(AHalloweenProps::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}

	if (GridManager)
	{
		PuzzleManager->BindToGrid(GridManager);
		if (SaveGame)
		{
			GridManager->SetGridSize(SaveGame->CourseWidth, SaveGame->CourseHeight);
		}
	}
	if (SaveGame)
	{
		PuzzleManager->SetExtrasEnabled(SaveGame->bOptionRelics);
		PuzzleManager->bBonusTilesEnabled = SaveGame->bOptionBonusTiles;
	}

	if (InputHandler)
	{
		InputHandler->GridManager = GridManager;
		InputHandler->PuzzleManager = PuzzleManager;
	}

	PuzzleManager->OnPiecePlaced.AddUObject(this, &APuzzleGameMode::HandlePiecePlaced);
	PuzzleManager->OnCleared.AddUObject(this, &APuzzleGameMode::HandleCleared);
	PuzzleManager->OnComboBroken.AddUObject(this, &APuzzleGameMode::HandleComboBroken);
	PuzzleManager->OnRelicGained.AddUObject(this, &APuzzleGameMode::HandleRelicGained);
	PuzzleManager->OnBonusSpawned.AddUObject(this, &APuzzleGameMode::HandleBonusSpawned);
	PuzzleManager->OnFinished.AddUObject(this, &APuzzleGameMode::HandleFinished);

	BotPlayer = NewObject<UPuzzleBotPlayer>(this);
	BotPlayer->Init(GridManager, PuzzleManager);

	// -grid=WxH: the board size, each side 4 to 8 (for example -grid=5x7).
	FString GridText;
	if (GridManager && FParse::Value(FCommandLine::Get(), TEXT("grid="), GridText))
	{
		FString WidthText, HeightText;
		if (GridText.Split(TEXT("x"), &WidthText, &HeightText))
		{
			GridManager->SetGridSize(FCString::Atoi(*WidthText), FCString::Atoi(*HeightText));
		}
	}

	ReframeCamera();

	PlayNextTrack();

	// -demo: auto-play with the bot.
	const TCHAR* CommandLine = FCommandLine::Get();
	float RecordSeconds = 0.f;
	if (FParse::Value(CommandLine, TEXT("recordaudio="), RecordSeconds) && RecordSeconds > 0.f)
	{
		StartAudioRecording(RecordSeconds);
	}
	if (FParse::Param(CommandLine, TEXT("demo")))
	{
		StartEndless();
		SetAutoPlay(true);
	}
	else
	{
		StartRound();
		ShowMenu();
		// First launch (or -tutorial): open the "How to play" pages over the menu.
		const bool bForceTutorial = FParse::Param(CommandLine, TEXT("tutorial"));
		if (SaveGame && (!SaveGame->bSeenTutorial || bForceTutorial))
		{
			if (UPuzzleHUDWidget* UI = GetUI())
			{
				int32 Page = 0;
				FParse::Value(CommandLine, TEXT("tutorialpage="), Page);
				UI->ShowTutorial(Page);
			}
			SaveGame->bSeenTutorial = true;
			SaveGame->Save();
		}
	}
}

void APuzzleGameMode::ReframeCamera()
{
	if (APuzzleCameraPawn* CameraPawn = Cast<APuzzleCameraPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (GridManager)
		{
			CameraPawn->SetFramingBounds(GridManager->GetContentBounds(), GridManager->GetPickPlaneZ());
		}
	}
}

UPuzzleHUDWidget* APuzzleGameMode::GetUI() const
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const APuzzleHUD* HUD = PC ? Cast<APuzzleHUD>(PC->GetHUD()) : nullptr;
	return HUD ? HUD->GetWidget() : nullptr;
}

void APuzzleGameMode::Shake(float Strength) const
{
	if (APuzzleCameraPawn* CameraPawn = Cast<APuzzleCameraPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		CameraPawn->AddShake(Strength);
	}
}

void APuzzleGameMode::Haptic(float Intensity, float Duration) const
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->PlayDynamicForceFeedback(FMath::Clamp(Intensity, 0.f, 1.f), Duration, true, true, true, true);
	}
}

void APuzzleGameMode::PlaySfx(USoundBase* Sound, float Volume, float Pitch) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound, Volume * SfxVolume, Pitch);
	}
}

void APuzzleGameMode::PlayNextTrack()
{
	// Loop lengths of the synthesized tracks (their wave assets loop, so the engine reports no duration).
	// Gain evens out their loudness (the mastered organ piece is ~3 dB hotter than the chant).
	struct FTrack { USoundBase* Sound; float Seconds; float Gain; };
	const FTrack Playlist[] = { { MusicSound, 106.f * 2.f, 1.f }, { OrganMusicSound, 64.f, 0.72f } };
	const FTrack& Track = Playlist[MusicTrackIndex % UE_ARRAY_COUNT(Playlist)];
	++MusicTrackIndex;
	if (!Track.Sound)
	{
		return;
	}

	constexpr float Crossfade = 5.f;
	if (MusicComponent)
	{
		MusicComponent->FadeOut(Crossfade, 0.f);
	}
	const float Volume = MusicVolume * Track.Gain;
	MusicComponent = UGameplayStatics::SpawnSound2D(this, Track.Sound, Volume, 1.f, 0.f, nullptr, true, true);
	if (MusicComponent && MusicTrackIndex > 1)
	{
		MusicComponent->FadeIn(Crossfade, Volume);
	}
	GetWorldTimerManager().SetTimer(MusicTimerHandle, this, &APuzzleGameMode::PlayNextTrack, Track.Seconds - Crossfade, false);
}

void APuzzleGameMode::Later(float Delay, TFunction<void()> Callback)
{
	FTimerHandle& Handle = RoundTimers.AddDefaulted_GetRef();
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Callback)), FMath::Max(Delay, 0.01f), false);
}

// --- Flow --------------------------------------------------------------------

void APuzzleGameMode::StartRound()
{
	for (FTimerHandle& Handle : RoundTimers)
	{
		GetWorldTimerManager().ClearTimer(Handle);
	}
	RoundTimers.Reset();

	bGameOver = false;
	bPauseMenuOpen = false;
	if (GridManager)
	{
		GridManager->InitBoard();
	}
	if (PuzzleManager)
	{
		PuzzleManager->StartGame();
	}
	if (InputHandler)
	{
		InputHandler->CancelInteraction();
	}
	if (UPuzzleHUDWidget* UI = GetUI())
	{
		UI->ResetRound();
	}
}

void APuzzleGameMode::ShowMenu()
{
	for (FTimerHandle& Handle : RoundTimers)
	{
		GetWorldTimerManager().ClearTimer(Handle);
	}
	RoundTimers.Reset();

	Flow = EPuzzleFlow::Menu;
	bPauseMenuOpen = false;
	if (InputHandler)
	{
		InputHandler->CancelInteraction();
	}
	if (UPuzzleHUDWidget* UI = GetUI())
	{
		UI->ShowCard(EPuzzleCard::Menu);
	}
}

void APuzzleGameMode::StartEndless()
{
	StartRound();
	Flow = EPuzzleFlow::Playing;
	if (UPuzzleHUDWidget* UI = GetUI())
	{
		UI->ShowCard(EPuzzleCard::None);
	}
}

void APuzzleGameMode::Retry()
{
	StartEndless();
}

void APuzzleGameMode::OpenPauseMenu()
{
	if (Flow != EPuzzleFlow::Playing || bGameOver)
	{
		return;
	}
	bPauseMenuOpen = true;
	if (InputHandler)
	{
		InputHandler->CancelInteraction();
	}
	if (UPuzzleHUDWidget* UI = GetUI())
	{
		UI->ShowCard(EPuzzleCard::Pause);
	}
}

void APuzzleGameMode::ClosePauseMenu()
{
	bPauseMenuOpen = false;
	if (UPuzzleHUDWidget* UI = GetUI())
	{
		UI->ShowCard(EPuzzleCard::None);
	}
}

void APuzzleGameMode::TogglePauseMenu()
{
	if (bPauseMenuOpen)
	{
		ClosePauseMenu();
	}
	else
	{
		OpenPauseMenu();
	}
}

void APuzzleGameMode::TakeOver()
{
	SetAutoPlay(false);
	ClosePauseMenu();
}

void APuzzleGameMode::StartDemo()
{
	StartEndless();
	SetAutoPlay(true);
}

void APuzzleGameMode::SetCourse(int32 Width, int32 Height)
{
	if (!GridManager)
	{
		return;
	}
	GridManager->SetGridSize(Width, Height);
	if (SaveGame)
	{
		SaveGame->CourseWidth = GridManager->GridWidth;
		SaveGame->CourseHeight = GridManager->GridHeight;
		SaveGame->Save();
	}
	ReframeCamera();
	// A fresh, empty board of the new size sits behind the menu.
	StartRound();
}

void APuzzleGameMode::SetOptions(bool bRelics, bool bBonusTiles)
{
	if (PuzzleManager)
	{
		PuzzleManager->SetExtrasEnabled(bRelics);
		PuzzleManager->bBonusTilesEnabled = bBonusTiles;
	}
	if (SaveGame)
	{
		SaveGame->bOptionRelics = bRelics;
		SaveGame->bOptionBonusTiles = bBonusTiles;
		SaveGame->Save();
	}
	StartRound();
}

bool APuzzleGameMode::IsBoardInputEnabled() const
{
	return Flow == EPuzzleFlow::Playing && !bGameOver && !bAutoPlayEnabled && !bPauseMenuOpen;
}

void APuzzleGameMode::RequestRelic(ERelic Relic)
{
	if (!IsBoardInputEnabled() || !PuzzleManager)
	{
		return;
	}
	if (Relic == ERelic::Reroll)
	{
		if (PuzzleManager->UseReroll())
		{
			PlaySfx(RelicSound, 0.8f, 1.25f);
			Haptic(0.3f, 0.15f);
		}
	}
	else if (InputHandler && PuzzleManager->GetRelicCharges(ERelic::HolyLight) > 0)
	{
		InputHandler->ToggleHolyTargeting();
	}
}

// --- Rule events -> feedback ---------------------------------------------------
// Feedback is delayed to match the visuals: pieces fly in for ArriveDuration before they land and clears pop.

void APuzzleGameMode::HandlePiecePlaced()
{
	Later(AGridManager::ArriveDuration, [this]()
	{
		Haptic(0.3f, 0.12f);
		PlaySfx(PlaceSound);
		Shake(1.5f);
	});
}

void APuzzleGameMode::HandleCleared(const FPuzzleClearEvent& Event)
{
	const float Delay = Event.bHolyLight ? 0.1f : AGridManager::ArriveDuration + 0.1f;
	Later(Delay, [this, Event]()
	{
		const int32 Lines = Event.Result.Lines;
		Haptic(Event.bHolyLight ? 0.9f : 0.45f + 0.15f * Lines, 0.3f);
		if (Event.bHolyLight)
		{
			PlaySfx(HolySound);
		}
		else
		{
			// The bell climbs a little with each combo step.
			PlaySfx(ClearSound, 1.f, FMath::Pow(2.f, FMath::Clamp(Event.Combo - 1, 0, 7) / 12.f));
		}
		if (Event.Result.Bonuses.Num() > 0)
		{
			PlaySfx(HolySound, 0.7f, 1.3f);
		}
		Shake(Event.bHolyLight ? 12.f : 5.f + 4.f * Lines);
		if (UPuzzleHUDWidget* UI = GetUI())
		{
			UI->ShowClear(Event);
		}
	});
}

void APuzzleGameMode::HandleComboBroken(int32 LostCombo)
{
	Later(AGridManager::ArriveDuration, [this, LostCombo]()
	{
		if (LostCombo >= 2)
		{
			PlaySfx(ComboLostSound, 0.7f);
		}
		if (UPuzzleHUDWidget* UI = GetUI())
		{
			UI->ShowComboBroken(LostCombo);
		}
	});
}

void APuzzleGameMode::HandleRelicGained(ERelic Relic)
{
	Later(AGridManager::ArriveDuration + 0.7f, [this, Relic]()
	{
		PlaySfx(RelicSound);
		Haptic(0.4f, 0.2f);
		if (UPuzzleHUDWidget* UI = GetUI())
		{
			UI->ShowRelicGained(Relic);
		}
	});
}

void APuzzleGameMode::HandleBonusSpawned(FIntPoint Cell)
{
	const FVector Where = GridManager ? GridManager->GetWorldLocationForCell(Cell.X, Cell.Y) : FVector::ZeroVector;
	// Matches the drop of the tile in AGridManager::SpawnBonusTile.
	Later(AGridManager::ArriveDuration + 0.6f + 0.45f, [this, Where]()
	{
		PlaySfx(RelicSound, 0.8f, 1.5f);
		Haptic(0.3f, 0.15f);
		if (UPuzzleHUDWidget* UI = GetUI())
		{
			UI->ShowBonusSpawned(Where);
		}
	});
}

void APuzzleGameMode::HandleFinished()
{
	bGameOver = true;
	Flow = EPuzzleFlow::Finished;
	if (InputHandler)
	{
		InputHandler->CancelInteraction();
	}

	bLastNewBest = false;
	// The demo bot's results never touch the player's progress.
	if (SaveGame && !bAutoPlayEnabled && PuzzleManager && PuzzleManager->Score > SaveGame->BestScore)
	{
		SaveGame->BestScore = PuzzleManager->Score;
		bLastNewBest = true;
		SaveGame->Save();
	}

	Later(AGridManager::ArriveDuration + 0.35f, [this]()
	{
		PlaySfx(GameOverSound);
		Shake(14.f);
		if (GridManager)
		{
			GridManager->CollapseBoard();
		}
	});

	Later(2.0f, [this]()
	{
		if (UPuzzleHUDWidget* UI = GetUI())
		{
			UI->ShowCard(EPuzzleCard::GameOver);
		}
	});

	if (bAutoPlayEnabled)
	{
		Later(2.0f + BotRestartDelay, [this]() { Retry(); });
	}
}

// --- Auto-play ---------------------------------------------------------------

void APuzzleGameMode::ToggleAutoPlay()
{
	SetAutoPlay(!bAutoPlayEnabled);
}

void APuzzleGameMode::SetAutoPlay(bool bEnabled)
{
	bAutoPlayEnabled = bEnabled;

	if (!bAutoPlayEnabled)
	{
		GetWorldTimerManager().ClearTimer(BotTimerHandle);
		return;
	}

	if (InputHandler)
	{
		InputHandler->CancelInteraction();
	}
	GetWorldTimerManager().SetTimer(BotTimerHandle, this, &APuzzleGameMode::BotTick, BotMoveInterval, true);

	// Take over from wherever the flow currently stands.
	switch (Flow)
	{
	case EPuzzleFlow::Menu:
		StartEndless();
		break;
	case EPuzzleFlow::Finished:
		Later(1.f, [this]() { Retry(); });
		break;
	default:
		break;
	}
}

void APuzzleGameMode::BotTick()
{
	if (bAutoPlayEnabled && BotPlayer && Flow == EPuzzleFlow::Playing && !bGameOver && !bPauseMenuOpen)
	{
		BotPlayer->TakeBestAction();
	}
}
