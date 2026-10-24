#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PuzzleTypes.h"
#include "CampaignData.h"
#include "PuzzleGameMode.generated.h"

class AGridManager;
class UPuzzleManager;
class APuzzleInputHandler;
class APuzzleHUD;
class UPuzzleHUDWidget;
class UPuzzleBotPlayer;
class UPuzzleSaveGame;
class UTrickOrTreat;
class USoundBase;
struct FPuzzleClearEvent;

UENUM(BlueprintType)
enum class EPuzzleFlow : uint8
{
	Menu,
	Playing,
	Finished
};

UCLASS()
class PUZZLEGAME5X5_API APuzzleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APuzzleGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	TObjectPtr<AGridManager> GridManager;

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	TObjectPtr<UPuzzleManager> PuzzleManager;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle")
	TSubclassOf<AGridManager> GridManagerClass;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle")
	TSubclassOf<APuzzleInputHandler> InputHandlerClass;

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	TObjectPtr<APuzzleInputHandler> InputHandler;

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	TObjectPtr<UPuzzleSaveGame> SaveGame;

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	TObjectPtr<UTrickOrTreat> TrickOrTreat;

	// --- The Trick-or-Treat packet (UTrickOrTreat): opened from its HUD button, then Treat (buy) or Trick (spin) ---
	void OpenPacket();
	// Closes the packet card; a packet that was neither bought nor spun stays for later.
	void ClosePacket();
	void ChooseTreat();
	void BuyTreat(int32 Offer);
	void ChooseTrick();
	// Called by the wheel when it stops on Segment.
	void FinishTrick(int32 Segment);

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	bool bPacketOpen = false;

	// --- Flow: menu -> playing -> finished card ---
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void ShowMenu();

	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void StartEndless();

	// Demo 1 (Mode 1: 4x4, no bonus tiles) or Demo 2 (Mode 2: 7x7, bonus tiles): sets the board and the options and starts a round.
	void StartDemoGame(int32 Mode);
	// Copies BestScore into the saved best of the demo mode being played (each mode keeps its own).
	void SyncDemoBest();

	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void Retry();

	// The menu that opens with P (or the MENU button) while playing: Take over / Resume and Main menu.
	void OpenPauseMenu();
	void ClosePauseMenu();
	void TogglePauseMenu();
	// Leaves demo mode and lets the player play the round that is on the board.
	void TakeOver();
	// Starts a fresh round with the bot playing.
	void StartDemo();
	// Picks the board size (each side 4 to 8), saves it and clears the board.
	void SetCourse(int32 Width, int32 Height);
	// The play options. Relics means combo, relics and luck together.
	void SetOptions(bool bRelics, bool bBonusTiles);

	// --- Campaigns: the arcade (challenges of escalating difficulty) and the tutorial (a step-by-step lesson), read from
	// arcade.json and tutorial.json (see FArcadeCampaign). Kind 0 is the arcade, 1 the tutorial. A "challenge" below is a step. ---
	bool HasCampaign(int32 Kind) const;
	bool IsArcade() const { return bArcade; } // inside a campaign
	int32 GetCampaignKind() const { return ActiveKind; }
	EArcadeState GetArcadeState() const { return ArcadeState; }
	const FArcadeChallenge* GetArcadeChallenge() const; // the step being played, null outside a campaign
	int32 GetArcadeIndex() const { return ArcadeIndex; }
	int32 GetArcadeCount() const;
	int32 GetArcadeProgress() const; // steps beaten in a row, saved
	float GetArcadeTimeLeft() const { return ArcadeTimeLeft; }
	const FArcadeCampaign* GetArcadeCampaign() const { return Campaigns[ActiveKind].Get(); }
	FString ArcadeFailReason;
	// From the main menu: offers to continue, or goes straight to the first step when nothing is beaten yet.
	void OpenCampaign(int32 Kind);
	void StartArcade(int32 FromIndex);
	void BeginArcadeChallenge(); // the START button of the intro popup: starts the clock
	void ArcadeAdvance();        // the NEXT button of the outro popup
	void ArcadeRetry();          // the RETRY button of the fail popup

	// Console command for trying a step's popups without playing it: PuzzleArcadeTest win | fail | time (5 seconds left).
	UFUNCTION(Exec)
	void PuzzleArcadeTest(const FString& What);

	// Test hook for the on-device interface sweep: shows one screen on demand. What is menu, courses, options, pause,
	// gameover, game, howto (A = page), arcademenu (A = campaign 0 arcade / 1 tutorial), arcadeintro (A = campaign,
	// B = step), arcadebegin, arcadewin, arcadefail, arcadetime, arcadedone (A = campaign). Nonce only makes each command text new, since the debug hook
	// runs a command once per distinct text.
	UFUNCTION(Exec)
	void PuzzleUiCard(const FString& What, int32 A = 0, int32 B = 0, int32 Nonce = 0);

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	bool bPauseMenuOpen = false;

	// Relic buttons: Reroll fires at once, Holy Light arms tap-to-target.
	void RequestRelic(ERelic Relic);

	// Human board input (drag, tap, gamepad) is only live while actually playing.
	bool IsBoardInputEnabled() const;

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	EPuzzleFlow Flow = EPuzzleFlow::Menu;

	// Result of the last finished round, for the end card.
	bool bLastNewBest = false;

	// --- Auto-play / demo mode: the bot plays itself on a fixed cadence ---
	UFUNCTION(BlueprintCallable, Category = "Puzzle|AI")
	void ToggleAutoPlay();

	UFUNCTION(BlueprintCallable, Category = "Puzzle|AI")
	void SetAutoPlay(bool bEnabled);

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle|AI")
	bool bAutoPlayEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	bool bGameOver = false;

	// Slow enough to follow each move: pieces take ~0.5s to fly in and clears ~1s to play out.
	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|AI")
	float BotMoveInterval = 1.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|AI")
	float BotRestartDelay = 3.5f;

	// --- Feedback: sounds, music and volumes ---
	// Defaults to the synthesized SFX in /Game/Audio; override in a Blueprint subclass to swap in real assets.
	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> PlaceSound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> ClearSound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> GameOverSound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> HolySound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> RelicSound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> ComboLostSound;

	// Soundtrack playlist: Gregorian chant (played twice), then the organ piece, crossfaded.
	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> MusicSound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> OrganMusicSound;

	// The demo build's tune, played in place of the chant (Tools/import_demo_music.py imports it). A soft reference, so the package
	// includes it and the game still runs where the asset is missing.
	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TSoftObjectPtr<USoundBase> DemoMusic;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	float MusicVolume = 0.7f;

	// Scales the sound effects the game mode plays, so the music stays in front. The cathedral's
	// thunder and candle-flicker sounds set their own volumes (AGothicEnvironment).
	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	float SfxVolume = 0.8f;

	// The volume sliders. They apply at once and are saved.
	void SetMusicVolume(float Volume);

	// Options card. Graphics Low is the lightweight scene (reloads the map, which builds the scenery once); Fullscreen
	// switches between a borderless full-screen window and an ordinary window (PC only).
	bool IsLowGraphics() const;
	void SetLowGraphics(bool bLow);
	bool IsFullscreen() const;
	int32 LastWindowMode = -1; // 1 fullscreen, 0 windowed; Tick watches it, since Alt+Enter and F11 change it too
	void SetFullscreen(bool bFullscreen);

	// The theme (Play options). Changing it reloads the map, which builds the tiles and the scenery in the new theme.
	EPuzzleTheme GetTheme() const;
	void SetTheme(EPuzzleTheme Theme);

	// Phones and tablets: the Landscape / Portrait choice of the options card (locks the screen that way).
	bool IsPortrait() const;
	void SetPortrait(bool bPortrait);
	void ApplyOrientation() const;
	void SetSfxVolume(float Volume);

	// Console commands for measuring performance on a device (they work in any build with a console).
	// PuzzleHide <backdrop|bats|steam|board|light|ui> <0|1>   hides (1) or shows (0) that part of the scene.
	UFUNCTION(Exec)
	void PuzzleHide(const FString& What, int32 Hide);

	// Dev command: the language of the instructions and the tutorial. PuzzleLang el | en | auto (auto = the system language). It applies to
	// the next card that is opened. There is no such setting for the player.
	UFUNCTION(Exec)
	void PuzzleLang(const FString& Language);

	// PuzzlePerf <max fps, 0 = no cap> <3D resolution in percent, 0 = leave> <1 = automatic resolution, 0 = fixed>
	// Sets the frame cap and the render resolution of the lightweight scene in one go, for power measurements.
	UFUNCTION(Exec)
	void PuzzlePerf(int32 MaxFps, int32 ResPercent, int32 Adaptive);

	UPuzzleHUDWidget* GetUI() const;

protected:
	void HandlePiecePlaced();
	void HandleCleared(const FPuzzleClearEvent& Event);
	void HandleComboBroken(int32 LostCombo);
	void HandleRelicGained(ERelic Relic);
	void HandleBonusSpawned(FIntPoint Cell);
	void HandleFinished();
	void HandlePacketArrived();
	// The bot opens the packet on its own: treat or trick (alternating, or -treatchoice=treat|trick).
	void BotOpenPacket();
	int32 BotPackets = 0;

	void LoadCampaign();
	void PrepareArcadeChallenge(bool bShowIntro);
	void LeaveArcade();
	void CheckArcadeWin();
	// The demo build ends a round at PuzzleDemo::ScoreCap() points with the demo-complete card.
	void CheckDemoCap();
	void FailArcade(const FString& Reason);

	TSharedPtr<FArcadeCampaign> Campaigns[2];
	int32 ActiveKind = 0;
	bool bArcade = false;
	int32 ArcadeIndex = 0;
	EArcadeState ArcadeState = EArcadeState::None;
	float ArcadeTimeLeft = 0.f;

	void ReframeCamera();
	void PlayNextTrack();

	void StartRound();
	void Later(float Delay, TFunction<void()> Callback);

	void BotTick();
	void Shake(float Strength) const;
	void Haptic(float Intensity, float Duration) const;
	void PlaySfx(USoundBase* Sound, float Volume = 1.f, float Pitch = 1.f) const;

	// -recordaudio=N: captures the game's audio mix to Saved/Recording/demo_audio.wav for N seconds, then quits.
	void StartAudioRecording(float Seconds);

	UPROPERTY()
	TObjectPtr<UPuzzleBotPlayer> BotPlayer;

	UPROPERTY()
	TObjectPtr<class UAudioComponent> MusicComponent;
	float MusicGain = 1.f; // the playing track's own gain

	UPROPERTY()
	TObjectPtr<class AGothicEnvironment> Environment;

	int32 MusicTrackIndex = 0;
	FTimerHandle MusicTimerHandle;

	FTimerHandle BotTimerHandle;
	// Every delayed callback of the current round, cleared when a new round starts.
	TArray<FTimerHandle> RoundTimers;
};
