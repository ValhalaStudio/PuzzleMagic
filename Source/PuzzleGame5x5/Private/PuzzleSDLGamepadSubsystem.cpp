#include "PuzzleSDLGamepadSubsystem.h"

#if WITH_SDL3
THIRD_PARTY_INCLUDES_START
#include "SDL3/SDL.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	constexpr Sint16 StickDeadzone = 8000;
}
#endif

void UPuzzleSDLGamepadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_SDL3
	if (!SDL_Init(SDL_INIT_GAMEPAD))
	{
		UE_LOG(LogTemp, Warning, TEXT("SDL3: failed to initialize gamepad subsystem: %s"), UTF8_TO_TCHAR(SDL_GetError()));
		return;
	}

	OpenFirstAvailableGamepad();
#endif
}

void UPuzzleSDLGamepadSubsystem::Deinitialize()
{
#if WITH_SDL3
	CloseActiveGamepad();
	SDL_Quit();
#endif

	Super::Deinitialize();
}

void UPuzzleSDLGamepadSubsystem::OpenFirstAvailableGamepad()
{
#if WITH_SDL3
	int32 NumGamepads = 0;
	SDL_JoystickID* GamepadIds = SDL_GetGamepads(&NumGamepads);
	if (GamepadIds && NumGamepads > 0)
	{
		ActiveGamepad = SDL_OpenGamepad(GamepadIds[0]);
	}
	if (GamepadIds)
	{
		SDL_free(GamepadIds);
	}
#endif
}

void UPuzzleSDLGamepadSubsystem::CloseActiveGamepad()
{
#if WITH_SDL3
	if (ActiveGamepad)
	{
		SDL_CloseGamepad(ActiveGamepad);
		ActiveGamepad = nullptr;
	}
#endif
}

void UPuzzleSDLGamepadSubsystem::PollEvents()
{
#if WITH_SDL3
	SDL_Event Event;
	while (SDL_PollEvent(&Event))
	{
		switch (Event.type)
		{
		case SDL_EVENT_GAMEPAD_ADDED:
			if (!ActiveGamepad)
			{
				ActiveGamepad = SDL_OpenGamepad(Event.gdevice.which);
			}
			break;
		case SDL_EVENT_GAMEPAD_REMOVED:
			if (ActiveGamepad && SDL_GetGamepadID(ActiveGamepad) == Event.gdevice.which)
			{
				CloseActiveGamepad();
				OpenFirstAvailableGamepad();
			}
			break;
		default:
			break;
		}
	}
#endif
}

void UPuzzleSDLGamepadSubsystem::RefreshButtonStates()
{
	PreviousButtonState = CurrentButtonState;

	if (!ActiveGamepad)
	{
		CurrentButtonState.Reset();
		return;
	}

#if WITH_SDL3
	CurrentButtonState.Add(EPuzzleGamepadButton::DPadUp, SDL_GetGamepadButton(ActiveGamepad, SDL_GAMEPAD_BUTTON_DPAD_UP));
	CurrentButtonState.Add(EPuzzleGamepadButton::DPadDown, SDL_GetGamepadButton(ActiveGamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN));
	CurrentButtonState.Add(EPuzzleGamepadButton::DPadLeft, SDL_GetGamepadButton(ActiveGamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT));
	CurrentButtonState.Add(EPuzzleGamepadButton::DPadRight, SDL_GetGamepadButton(ActiveGamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT));
	CurrentButtonState.Add(EPuzzleGamepadButton::Confirm, SDL_GetGamepadButton(ActiveGamepad, SDL_GAMEPAD_BUTTON_SOUTH));
	CurrentButtonState.Add(EPuzzleGamepadButton::Cancel, SDL_GetGamepadButton(ActiveGamepad, SDL_GAMEPAD_BUTTON_EAST));
	CurrentButtonState.Add(EPuzzleGamepadButton::CyclePiece, SDL_GetGamepadButton(ActiveGamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER));
#endif
}

void UPuzzleSDLGamepadSubsystem::Tick(float DeltaTime)
{
	PollEvents();
	RefreshButtonStates();
}

bool UPuzzleSDLGamepadSubsystem::IsGamepadConnected() const
{
	return ActiveGamepad != nullptr;
}

FVector2D UPuzzleSDLGamepadSubsystem::GetLeftStick() const
{
#if WITH_SDL3
	if (!ActiveGamepad)
	{
		return FVector2D::ZeroVector;
	}

	Sint16 RawX = SDL_GetGamepadAxis(ActiveGamepad, SDL_GAMEPAD_AXIS_LEFTX);
	Sint16 RawY = SDL_GetGamepadAxis(ActiveGamepad, SDL_GAMEPAD_AXIS_LEFTY);

	if (FMath::Abs(RawX) < StickDeadzone) RawX = 0;
	if (FMath::Abs(RawY) < StickDeadzone) RawY = 0;

	return FVector2D(RawX / 32768.f, -RawY / 32768.f);
#else
	return FVector2D::ZeroVector;
#endif
}

bool UPuzzleSDLGamepadSubsystem::WasButtonJustPressed(EPuzzleGamepadButton Button) const
{
	const bool bNow = CurrentButtonState.Contains(Button) && CurrentButtonState[Button];
	const bool bBefore = PreviousButtonState.Contains(Button) && PreviousButtonState[Button];
	return bNow && !bBefore;
}
