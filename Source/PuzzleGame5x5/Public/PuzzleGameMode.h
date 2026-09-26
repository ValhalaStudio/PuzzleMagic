#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PuzzleTypes.h"
#include "PuzzleGameMode.generated.h"

class AGridManager;
class UPuzzleManager;
class APuzzleInputHandler;
class APuzzleHUD;
class UPuzzleHUDWidget;
class UPuzzleBotPlayer;
class UPuzzleSaveGame;
class USoundBase;
struct FPuzzleClearEvent;

UENUM(BlueprintType)
enum class EPuzzleFlow : uint8
{
	Menu,
	LevelIntro,
	Playing,
	Finished
};

UCLASS()
class PUZZLEGAME5X5_API APuzzleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APuzzleGameMode();

	virtual void StartPlay() override;

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

	// --- Flow: menu -> (level intro) -> playing -> finished card ---
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void ShowMenu();

	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void StartEndless();

	// Sets up quest level N and shows its intro card.
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void SelectLevel(int32 Level);

	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void BeginLevel();

	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void NextLevel();

	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void Retry();

	// Relic buttons: Reroll fires at once, Holy Light arms tap-to-target.
	void RequestRelic(ERelic Relic);

	// Human board input (drag, tap, gamepad) is only live while actually playing.
	bool IsBoardInputEnabled() const;

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	EPuzzleFlow Flow = EPuzzleFlow::Menu;

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	EPlayMode Mode = EPlayMode::Endless;

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	int32 CurrentLevel = 1;

	// Result of the last finished round, for the end card.
	int32 LastStars = 0;
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
	TObjectPtr<USoundBase> BlessedSound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> HolySound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> GargoyleSound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> RelicSound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> ComboLostSound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> StrikeSound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> HexSound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> WardSound;

	// Soundtrack playlist: Gregorian chant (played twice), then the organ piece, crossfaded.
	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> MusicSound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	TObjectPtr<USoundBase> OrganMusicSound;

	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	float MusicVolume = 0.7f;

	// Scales the sound effects the game mode plays, so the music stays in front. The cathedral's
	// thunder and candle-flicker sounds set their own volumes (AGothicEnvironment).
	UPROPERTY(EditDefaultsOnly, Category = "Puzzle|Feedback")
	float SfxVolume = 0.8f;

	UPuzzleHUDWidget* GetUI() const;

protected:
	void HandlePiecePlaced();
	void HandleCleared(const FPuzzleClearEvent& Event);
	void HandleComboBroken(int32 LostCombo);
	void HandleRelicGained(ERelic Relic);
	void HandleStoneSpawned(FIntPoint Cell);
	void HandleOmen(EOmen Omen, bool bWarded, FIntPoint Cell);
	void HandleFinished(bool bWon);

	void PlayNextTrack();

	void StartRound();
	void PlayCelebration();
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

	UPROPERTY()
	TObjectPtr<class AGothicEnvironment> Environment;

	int32 MusicTrackIndex = 0;
	FTimerHandle MusicTimerHandle;

	FTimerHandle BotTimerHandle;
	// Every delayed callback of the current round, cleared when a new round starts.
	TArray<FTimerHandle> RoundTimers;
};
