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
#include "QuestCatalog.h"
#include "GothicEnvironment.h"
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
	static ConstructorHelpers::FObjectFinder<USoundBase> BlessedFinder(TEXT("/Game/Audio/SFXG_Blessed.SFXG_Blessed"));
	static ConstructorHelpers::FObjectFinder<USoundBase> HolyFinder(TEXT("/Game/Audio/SFXG_Holy.SFXG_Holy"));
	static ConstructorHelpers::FObjectFinder<USoundBase> GargoyleFinder(TEXT("/Game/Audio/SFXG_Gargoyle.SFXG_Gargoyle"));
	static ConstructorHelpers::FObjectFinder<USoundBase> RelicFinder(TEXT("/Game/Audio/SFXG_Relic.SFXG_Relic"));
	static ConstructorHelpers::FObjectFinder<USoundBase> ComboLostFinder(TEXT("/Game/Audio/SFXG_ComboLost.SFXG_ComboLost"));
	static ConstructorHelpers::FObjectFinder<USoundBase> StrikeFinder(TEXT("/Game/Audio/SFXG_Strike.SFXG_Strike"));
	static ConstructorHelpers::FObjectFinder<USoundBase> HexFinder(TEXT("/Game/Audio/SFXG_Hex.SFXG_Hex"));
	static ConstructorHelpers::FObjectFinder<USoundBase> WardFinder(TEXT("/Game/Audio/SFXG_Ward.SFXG_Ward"));
	static ConstructorHelpers::FObjectFinder<USoundBase> MusicFinder(TEXT("/Game/Audio/MUS_Chant.MUS_Chant"));
	static ConstructorHelpers::FObjectFinder<USoundBase> OrganFinder(TEXT("/Game/Audio/MUS_Gothic.MUS_Gothic"));
	StrikeSound = StrikeFinder.Object;
	HexSound = HexFinder.Object;
	WardSound = WardFinder.Object;
	OrganMusicSound = OrganFinder.Object;
	PlaceSound = PlaceFinder.Object;
	ClearSound = ClearFinder.Object;
	GameOverSound = GameOverFinder.Object;
	BlessedSound = BlessedFinder.Object;
	HolySound = HolyFinder.Object;
	GargoyleSound = GargoyleFinder.Object;
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
	}

	if (GridManager)
	{
		PuzzleManager->BindToGrid(GridManager);
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
	PuzzleManager->OnStoneSpawned.AddUObject(this, &APuzzleGameMode::HandleStoneSpawned);
	PuzzleManager->OnOmen.AddUObject(this, &APuzzleGameMode::HandleOmen);
	PuzzleManager->OnFinished.AddUObject(this, &APuzzleGameMode::HandleFinished);

	BotPlayer = NewObject<UPuzzleBotPlayer>(this);
	BotPlayer->Init(GridManager, PuzzleManager);

	if (APuzzleCameraPawn* CameraPawn = Cast<APuzzleCameraPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (GridManager)
		{
			CameraPawn->SetFramingBounds(GridManager->GetContentBounds(), GridManager->GetPickPlaneZ());
		}
	}

	PlayNextTrack();

	// -demo: endless auto-play. -demoquest[=N]: the bot plays through the quest levels from N.
	// -level=N: open quest level N's intro card directly.
	const TCHAR* CommandLine = FCommandLine::Get();
	float RecordSeconds = 0.f;
	if (FParse::Value(CommandLine, TEXT("recordaudio="), RecordSeconds) && RecordSeconds > 0.f)
	{
		StartAudioRecording(RecordSeconds);
	}
	// -fsr: AMD FSR upscaling (Quality, 1.5x) instead of TSR at native resolution, for large PC windows.
	// Off by default (Config/DefaultEngine.ini), and the plugin's frame interpolation stays off either way.
#if PLATFORM_WINDOWS
	if (FParse::Param(CommandLine, TEXT("fsr")))
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->ConsoleCommand(TEXT("r.FidelityFX.FSR.Enabled 1"));
			PC->ConsoleCommand(TEXT("r.FidelityFX.FSR.QualityMode 1"));
		}
	}
#endif
	float OmenRate = 0.f;
	if (FParse::Value(CommandLine, TEXT("omenrate="), OmenRate))
	{
		PuzzleManager->OmenChanceOverride = FMath::Clamp(OmenRate, 0.f, 1.f);
	}
	int32 Level = 0;
	if (FParse::Value(CommandLine, TEXT("demoquest="), Level) || FParse::Param(CommandLine, TEXT("demoquest")))
	{
		SelectLevel(FMath::Max(Level, 1));
		SetAutoPlay(true);
	}
	else if (FParse::Param(CommandLine, TEXT("demo")))
	{
		StartEndless();
		SetAutoPlay(true);
	}
	else if (FParse::Value(CommandLine, TEXT("level="), Level))
	{
		SelectLevel(Level);
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
	if (GridManager)
	{
		GridManager->InitBoard();
	}
	if (PuzzleManager)
	{
		PuzzleManager->StartGame(Mode, QuestCatalog::Get(CurrentLevel));
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
	Mode = EPlayMode::Endless;
	StartRound();
	Flow = EPuzzleFlow::Playing;
	if (UPuzzleHUDWidget* UI = GetUI())
	{
		UI->ShowCard(EPuzzleCard::None);
	}
}

void APuzzleGameMode::SelectLevel(int32 Level)
{
	Mode = EPlayMode::Quest;
	CurrentLevel = FMath::Clamp(Level, 1, QuestCatalog::Levels().Num());
	StartRound();
	Flow = EPuzzleFlow::LevelIntro;
	if (UPuzzleHUDWidget* UI = GetUI())
	{
		UI->ShowCard(EPuzzleCard::LevelIntro);
	}
	if (bAutoPlayEnabled)
	{
		Later(2.4f, [this]() { BeginLevel(); });
	}
}

void APuzzleGameMode::BeginLevel()
{
	if (Flow != EPuzzleFlow::LevelIntro)
	{
		return;
	}
	Flow = EPuzzleFlow::Playing;
	if (UPuzzleHUDWidget* UI = GetUI())
	{
		UI->ShowCard(EPuzzleCard::None);
	}
}

void APuzzleGameMode::NextLevel()
{
	if (CurrentLevel >= QuestCatalog::Levels().Num())
	{
		ShowMenu();
		return;
	}
	SelectLevel(CurrentLevel + 1);
}

void APuzzleGameMode::Retry()
{
	if (Mode == EPlayMode::Quest)
	{
		SelectLevel(CurrentLevel);
	}
	else
	{
		StartEndless();
	}
}

bool APuzzleGameMode::IsBoardInputEnabled() const
{
	return Flow == EPuzzleFlow::Playing && !bGameOver && !bAutoPlayEnabled;
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
			PlaySfx(ClearSound, 1.f, FMath::Pow(2.f, FMath::Min(Event.Combo - 1, 7) / 12.f));
		}
		if (Event.Result.Blessings > 0 || Event.Result.bBlessedBox)
		{
			PlaySfx(BlessedSound, 0.9f);
		}
		if (Event.Result.StonesBroken > 0)
		{
			PlaySfx(GargoyleSound, 0.6f, 1.35f);
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

void APuzzleGameMode::HandleStoneSpawned(FIntPoint Cell)
{
	const FVector Where = GridManager ? GridManager->GetWorldLocationForCell(Cell.X, Cell.Y) : FVector::ZeroVector;
	// Matches the stone's fall in AGridManager::SpawnCurseStone.
	Later(AGridManager::ArriveDuration + 0.7f + 0.45f, [this, Where]()
	{
		PlaySfx(GargoyleSound);
		Haptic(0.6f, 0.2f);
		Shake(8.f);
		if (UPuzzleHUDWidget* UI = GetUI())
		{
			UI->ShowStoneLanded(Where);
		}
	});
}

void APuzzleGameMode::HandleOmen(EOmen Omen, bool bWarded, FIntPoint Cell)
{
	const float Delay = UPuzzleManager::OmenDelay;
	if (Environment && Omen == EOmen::Lightning)
	{
		// The storm outside flashes through the stained glass with the strike (scheduled for Delay).
		Environment->TriggerStrike(Delay);
	}
	const FVector Where = GridManager ? GridManager->GetWorldLocationForCell(Cell.X, Cell.Y) : FVector::ZeroVector;
	Later(Delay, [this, Omen, bWarded, Where]()
	{
		if (Environment)
		{
			// Even a warded omen stirs the things in the dark a little, as it lands.
			Environment->AddDread(bWarded ? 0.1f : 0.35f);
		}
		if (Omen == EOmen::Lightning)
		{
			PlaySfx(StrikeSound, 1.f, FMath::FRandRange(0.92f, 1.06f));
			Shake(bWarded ? 7.f : 15.f);
			Haptic(0.9f, 0.3f);
		}
		else
		{
			PlaySfx(HexSound);
			Shake(4.f);
			Haptic(0.5f, 0.4f);
		}
		if (UPuzzleHUDWidget* UI = GetUI())
		{
			UI->ShowOmen(Omen, bWarded, Where);
		}
	});
	if (bWarded)
	{
		// Matches the golden ward in AGridManager::StrikeLightning / PlayHex.
		Later(Delay + (Omen == EOmen::Hex ? 0.55f : 0.05f), [this]()
		{
			PlaySfx(WardSound, 0.9f);
			Haptic(0.3f, 0.2f);
		});
	}
}

void APuzzleGameMode::HandleFinished(bool bWon)
{
	bGameOver = true;
	Flow = EPuzzleFlow::Finished;
	if (InputHandler)
	{
		InputHandler->CancelInteraction();
	}

	LastStars = PuzzleManager ? PuzzleManager->ComputeStars() : 0;
	bLastNewBest = false;
	// The demo bot's results never touch the player's progress.
	if (SaveGame && !bAutoPlayEnabled)
	{
		if (Mode == EPlayMode::Quest && bWon)
		{
			SaveGame->RecordStars(CurrentLevel, LastStars);
			SaveGame->HighestUnlockedLevel = FMath::Max(SaveGame->HighestUnlockedLevel, FMath::Min(CurrentLevel + 1, QuestCatalog::Levels().Num()));
			SaveGame->Save();
		}
		else if (Mode == EPlayMode::Endless && PuzzleManager && PuzzleManager->Score > SaveGame->BestEndlessScore)
		{
			SaveGame->BestEndlessScore = PuzzleManager->Score;
			bLastNewBest = true;
			SaveGame->Save();
		}
	}

	Later(AGridManager::ArriveDuration + 0.35f, [this, bWon]()
	{
		if (bWon)
		{
			PlayCelebration();
		}
		else
		{
			PlaySfx(GameOverSound);
			Shake(14.f);
		}
		if (GridManager)
		{
			GridManager->CollapseBoard();
		}
	});

	Later(2.0f, [this, bWon]()
	{
		if (UPuzzleHUDWidget* UI = GetUI())
		{
			UI->ShowCard(Mode == EPlayMode::Endless ? EPuzzleCard::EndlessOver : (bWon ? EPuzzleCard::LevelComplete : EPuzzleCard::LevelFailed));
		}
	});

	if (bAutoPlayEnabled)
	{
		Later(2.0f + BotRestartDelay, [this, bWon]()
		{
			if (Mode == EPlayMode::Quest && bWon)
			{
				NextLevel();
			}
			else
			{
				Retry();
			}
		});
	}
}

void APuzzleGameMode::PlayCelebration()
{
	PlaySfx(HolySound);
	PlaySfx(BlessedSound, 0.8f);
	Haptic(0.8f, 0.5f);
	Shake(10.f);
	if (!GridManager || !GetWorld())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	const FVector Centre = GridManager->GetActorLocation() + FVector(0.f, 0.f, GridManager->GetPickPlaneZ() + 30.f);
	if (APuzzleFX* FX = GetWorld()->SpawnActor<APuzzleFX>(APuzzleFX::StaticClass(), Centre, FRotator::ZeroRotator, SpawnParams))
	{
		const FLinearColor Gold = FLinearColor(1.f, 0.8f, 0.35f) * 2.2f;
		for (int32 Ring = 0; Ring < 4; ++Ring)
		{
			FX->AddRing(Centre, 200.f + 170.f * Ring, Ring % 2 ? FLinearColor(0.8f, 0.5f, 1.f) * 2.f : Gold, Ring * 0.12f);
		}
		for (int32 Sparkle = 0; Sparkle < 40; ++Sparkle)
		{
			const FVector Offset(FMath::FRandRange(-420.f, 420.f), FMath::FRandRange(-420.f, 420.f), FMath::FRandRange(0.f, 60.f));
			const FLinearColor Color = PuzzleTypes::ToLinearColor(static_cast<EPuzzleTileColor>(Sparkle % PuzzleColorCount)) * 1.5f + FLinearColor(0.3f, 0.3f, 0.3f);
			FX->AddSparkle(Centre + Offset, Color, FMath::FRandRange(0.f, 0.9f));
		}
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
	case EPuzzleFlow::LevelIntro:
		Later(2.4f, [this]() { BeginLevel(); });
		break;
	case EPuzzleFlow::Finished:
		Later(1.f, [this]()
		{
			if (Mode == EPlayMode::Quest && PuzzleManager && PuzzleManager->HasWon()) { NextLevel(); } else { Retry(); }
		});
		break;
	default:
		break;
	}
}

void APuzzleGameMode::BotTick()
{
	if (bAutoPlayEnabled && BotPlayer && Flow == EPuzzleFlow::Playing && !bGameOver)
	{
		BotPlayer->TakeBestAction();
	}
}
