// Lightweight (mobile) mode of the Halloween scenery: a pre-rendered backdrop picture and flat sprites (bats,
// steam) that ride on the camera, instead of the real-time 3D cauldrons, mist and flying quads of HalloweenProps.cpp.
#include "HalloweenProps.h"
#include "PuzzleCameraPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "HAL/IConsoleManager.h"
#include "PuzzleLite.h"
#include "PuzzleStyle.h"

namespace
{
	// What the Low graphics mode changed, so a switch back to High on a PC can undo it (the settings outlive the map).
	TMap<FString, FString> GSavedSettings;

	void SetSaved(APlayerController* PC, const FString& Name, const FString& Value)
	{
		if (!GSavedSettings.Contains(Name))
		{
			if (const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name))
			{
				GSavedSettings.Add(Name, Variable->GetString());
			}
		}
		PC->ConsoleCommand(Name + TEXT(" ") + Value);
	}
}

namespace
{
	// Distances in front of the camera, farthest first: the backdrop, then steam, then bats. The board sits nearer than all.
	constexpr float BackdropDepth = 2600.f;
	constexpr float SteamDepth = 2500.f;
	constexpr float BatDepth = 2450.f;

	// Where the two cauldrons stand in the backdrop picture (-1..1 across and up).
	const FVector2D CauldronMouth[2] = { FVector2D(-0.80f, 0.33f), FVector2D(0.80f, 0.33f) };
}

UStaticMeshComponent* AHalloweenProps::AddLiteQuad(UMaterialInterface* Material)
{
	UStaticMeshComponent* Quad = NewObject<UStaticMeshComponent>(this);
	Quad->SetStaticMesh(PlaneMesh);
	Quad->SetupAttachment(RootComponent);
	Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Quad->SetCastShadow(false);
	Quad->SetReceivesDecals(false);
	Quad->RegisterComponent();
	Owned.Add(Quad);
	// Its normal points back at the camera (local -X), its width runs to the right and its height down the screen.
	Quad->SetRelativeRotation(FRotationMatrix::MakeFromZX(FVector(-1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f)).Rotator());
	if (Material)
	{
		Quad->SetMaterial(0, Material);
	}
	return Quad;
}

void AHalloweenProps::PlaceOnBackdrop(UStaticMeshComponent* Quad, const FVector2D& ImageUV, float Depth, float HeightFraction, float WidthOverHeight, bool bFlipX)
{
	// The backdrop picture covers the view (the longer side is cropped), so a spot of the picture, in -1..1 across and
	// up, lands at a fixed place in front of the camera whatever the screen's shape.
	const float TanHalfW = FMath::Tan(FMath::DegreesToRadians(LiteFOV * 0.5f));
	float HalfH = Depth * TanHalfW / LiteAspect;
	float HalfW = Depth * TanHalfW;
	if (LiteAspect < LiteImageAspect)
	{
		HalfH = Depth * TanHalfW / LiteAspect;
		HalfW = HalfH * LiteImageAspect;
	}
	else
	{
		HalfH = HalfW / LiteImageAspect;
	}
	const float Height = HeightFraction * 2.f * HalfH;
	const float Width = Height * WidthOverHeight;
	Quad->SetRelativeLocation(FVector(Depth, ImageUV.X * HalfW, ImageUV.Y * HalfH));
	Quad->SetRelativeScale3D(FVector(FMath::Max(Width, 1.f) / 100.f * (bFlipX ? -1.f : 1.f), FMath::Max(Height, 1.f) / 100.f, 1.f));
}

void AHalloweenProps::SetupLitePerformance()
{
	// Screen-wide effects that are too dear for a weak mobile GPU.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PuzzleLite::IsMobile())
		{
			PC->ConsoleCommand(TEXT("t.MaxFPS 30"));
		}
		for (const TCHAR* Name : { TEXT("r.Mobile.AntiAliasing"), TEXT("r.BloomQuality"), TEXT("r.LensFlareQuality"), TEXT("r.SceneColorFringeQuality"), TEXT("r.FilmGrain") })
		{
			SetSaved(PC, Name, TEXT("0"));
		}
		if (!PuzzleLite::IsMobile())
		{
			SetSaved(PC, TEXT("r.ScreenPercentage"), TEXT("100"));
		}
	}
	// A phone starts at 70% and adapts; a PC in Low graphics starts at full resolution and only drops if it is slow.
	LiteResolution = PuzzleLite::IsMobile() ? 0.7f : 1.f;
	ApplyLiteResolution();
}

void AHalloweenProps::ApplyLiteResolution()
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->ConsoleCommand(FString::Printf(TEXT("r.ScreenPercentage %d"), FMath::RoundToInt(LiteResolution * 100.f)));
	}
}

void AHalloweenProps::RestoreAfterLite()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}
	for (const TPair<FString, FString>& Setting : GSavedSettings)
	{
		PC->ConsoleCommand(Setting.Key + TEXT(" ") + Setting.Value);
	}
	GSavedSettings.Reset();
}

float AHalloweenProps::ComputeLiteDepthScale() const
{
	const APuzzleCameraPawn* Pawn = Cast<APuzzleCameraPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Pawn)
	{
		return 1.f;
	}
	// The board and tray reach a little past the framed point towards the camera's far side: keep a margin behind them.
	return FMath::Max(1.f, Pawn->GetFramingDistance() * 1.3f / BatDepth);
}

void AHalloweenProps::SetPerfOverride(bool bAdapt, float Resolution)
{
	bAdaptResolution = bAdapt;
	if (Resolution > 0.f)
	{
		LiteResolution = FMath::Clamp(Resolution, 0.35f, 1.f);
		ApplyLiteResolution();
	}
	// A fresh measuring window when the automatic resolution is switched back on.
	LiteWindowTime = 0.f;
	LiteWindowWorst = 0.f;
	LiteWindowFrames = 0;
}

void AHalloweenProps::AdaptResolution(float DeltaTime)
{
	if (!bAdaptResolution)
	{
		return;
	}
	// A phone or tablet is capped at 30 frames a second (about a fifth less power than running free, measured), so 30 is its
	// best case: it counts as slow below 27.5 and the resolution only goes back up while it holds the cap. A PC in Low
	// graphics aims for 60 (held there by vsync, so it must not read as slow above 50).
	const bool bMobile = PuzzleLite::IsMobile();
	const float TargetFps = bMobile ? 30.f : 62.f;
	const float SlowFps = bMobile ? 27.5f : 50.f;
	const float RaiseFps = bMobile ? 29.4f : TargetFps * 1.45f;
	constexpr float WindowSeconds = 2.5f;
	constexpr float MinResolution = 0.35f;

	// Skip the load hitches at the start.
	LiteWarmup += DeltaTime;
	if (LiteWarmup < 6.f)
	{
		return;
	}
	const float Frame = FApp::GetDeltaTime();
	LiteWindowTime += Frame;
	LiteWindowWorst = FMath::Max(LiteWindowWorst, Frame);
	++LiteWindowFrames;
	if (LiteWindowTime < WindowSeconds)
	{
		return;
	}

	const float Fps = LiteWindowFrames / LiteWindowTime;
	const bool bHitch = LiteWindowWorst > 0.6f; // a pause or the app in the background, not the GPU
	LiteWindowTime = 0.f;
	LiteWindowWorst = 0.f;
	LiteWindowFrames = 0;
	if (bHitch)
	{
		return;
	}

	if (Fps < SlowFps && LiteResolution > MinResolution)
	{
		// Slow: the cost is mostly per pixel, so the cost of the pixel work scales with the resolution squared.
		if (LiteLastRaiseFrom > 0.f && LiteResolution > LiteLastRaiseFrom)
		{
			LiteCeiling = LiteLastRaiseFrom; // the last step up was too much
		}
		LiteResolution = FMath::Clamp(LiteResolution * FMath::Sqrt(FMath::Max(Fps / TargetFps, 0.5f)), MinResolution, LiteCeiling);
		ApplyLiteResolution();
	}
	else if (Fps > RaiseFps && LiteResolution < FMath::Min(1.f, LiteCeiling))
	{
		LiteLastRaiseFrom = LiteResolution;
		LiteResolution = FMath::Min(LiteResolution + 0.05f, FMath::Min(1.f, LiteCeiling));
		ApplyLiteResolution();
	}
}

void AHalloweenProps::RefreshLiteFrame()
{
	FVector2D Viewport(16.f, 9.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(Viewport);
	}
	if (Viewport.X <= 0.f || Viewport.Y <= 0.f)
	{
		return;
	}
	const APuzzleCameraPawn* Pawn = Cast<APuzzleCameraPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	const float Fov = Pawn ? Pawn->LandscapeFOV : 50.f;
	const float Aspect = Viewport.X / Viewport.Y;
	const float DepthScale = ComputeLiteDepthScale();
	if (FMath::IsNearlyEqual(Aspect, LiteAspect, 0.001f) && FMath::IsNearlyEqual(Fov, LiteFOV, 0.01f) && FMath::IsNearlyEqual(DepthScale, LiteDepthScale, 0.01f))
	{
		return;
	}
	LiteAspect = Aspect;
	LiteFOV = Fov;
	LiteDepthScale = DepthScale;
	if (LiteBackdrop)
	{
		PlaceOnBackdrop(LiteBackdrop, FVector2D::ZeroVector, BackdropDepth * LiteDepthScale, 1.f, LiteImageAspect, false);
	}
}

void AHalloweenProps::BuildLite()
{
	APuzzleCameraPawn* Pawn = Cast<APuzzleCameraPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("HalloweenProps: no camera pawn, the lightweight scenery is skipped"));
		return;
	}
	// Everything here is in the camera's own space (forward is +X), so the sprites stay put on the screen.
	AttachToActor(Pawn, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetupLitePerformance();

	UMaterialInterface* BackdropMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_LiteBackdrop.M_LiteBackdrop"));
	UMaterialInterface* SpriteMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_LiteSprite.M_LiteSprite"));
	// Each cathedral theme has its own picture (a theme with props also has the bat and steam sprites); a scene made of real
	// geometry (the desk) has no picture at all.
	const FPuzzleThemeDef& Theme = PuzzleStyle::Current();
	UTexture2D* BackdropTexture = Theme.LiteBackdrop ? LoadObject<UTexture2D>(nullptr, Theme.LiteBackdrop) : nullptr;
	if (!BackdropTexture && Theme.LiteBackdrop)
	{
		BackdropTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Lite/T_LiteBackdrop.T_LiteBackdrop"));
	}
	UTexture2D* BatTexture = Theme.bProps ? LoadObject<UTexture2D>(nullptr, TEXT("/Game/Lite/T_LiteBat.T_LiteBat")) : nullptr;
	UTexture2D* SteamTexture = Theme.bProps ? LoadObject<UTexture2D>(nullptr, TEXT("/Game/Lite/T_LiteSteam.T_LiteSteam")) : nullptr;
	if (!BackdropMaterial || !SpriteMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("HalloweenProps: lightweight materials are missing (run Tools/build_lite_assets.py)"));
		return;
	}

	RefreshLiteFrame();

	if (BackdropTexture)
	{
		UMaterialInstanceDynamic* Back = UMaterialInstanceDynamic::Create(BackdropMaterial, this);
		Back->SetTextureParameterValue(TEXT("Tex"), BackdropTexture);
		LiteBackdrop = AddLiteQuad(Back);
		PlaceOnBackdrop(LiteBackdrop, FVector2D::ZeroVector, BackdropDepth * LiteDepthScale, 1.f, LiteImageAspect, false);
	}

	// Steam: five puffs over each cauldron, each rising and thinning out in turn.
	if (SteamTexture)
	{
		for (int32 Side = 0; Side < 2; ++Side)
		{
			for (int32 I = 0; I < 5; ++I)
			{
				FLiteSteam& Puff = LiteSteam.AddDefaulted_GetRef();
				Puff.Anchor = CauldronMouth[Side];
				Puff.Life = FMath::FRandRange(3.2f, 4.4f);
				Puff.Age = Puff.Life * I / 5.f;
				Puff.Size = FMath::FRandRange(0.2f, 0.28f);
				Puff.Drift = FMath::FRandRange(-0.05f, 0.05f);
				Puff.MID = UMaterialInstanceDynamic::Create(SpriteMaterial, this);
				Puff.MID->SetTextureParameterValue(TEXT("Tex"), SteamTexture);
				Puff.MID->SetScalarParameterValue(TEXT("Cols"), 4.f);
				Puff.MID->SetScalarParameterValue(TEXT("Rows"), 4.f);
				Puff.MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.7f, 1.f, 0.4f));
				Puff.MID->SetScalarParameterValue(TEXT("Opacity"), 0.f);
				Puff.Mesh = AddLiteQuad(Puff.MID);
			}
		}
	}

	// Bats: six circle above the cauldrons, two cross the top of the picture.
	if (BatTexture)
	{
		for (int32 I = 0; I < 8; ++I)
		{
			FLiteBat& Bat = LiteBats.AddDefaulted_GetRef();
			Bat.bCrossing = I >= 6;
			if (Bat.bCrossing)
			{
				Bat.Centre = FVector2D(0.f, 0.74f + 0.08f * (I - 6));
				Bat.Radius = FVector2D(1.15f, 0.05f);
				Bat.Speed = FMath::FRandRange(0.11f, 0.16f);
			}
			else
			{
				Bat.Centre = FVector2D((I % 2 == 0) ? CauldronMouth[0].X : CauldronMouth[1].X, 0.5f + 0.07f * (I / 2));
				Bat.Radius = FVector2D(FMath::FRandRange(0.12f, 0.2f), FMath::FRandRange(0.06f, 0.11f));
				Bat.Speed = FMath::FRandRange(0.5f, 0.9f) * (FMath::RandBool() ? 1.f : -1.f);
			}
			Bat.Phase = FMath::FRandRange(0.f, 2.f * PI);
			Bat.FlapRate = FMath::FRandRange(8.f, 12.f);
			Bat.Size = FMath::FRandRange(0.075f, 0.105f);
			Bat.MID = UMaterialInstanceDynamic::Create(SpriteMaterial, this);
			Bat.MID->SetTextureParameterValue(TEXT("Tex"), BatTexture);
			Bat.MID->SetScalarParameterValue(TEXT("Cols"), 4.f);
			Bat.MID->SetScalarParameterValue(TEXT("Rows"), 1.f);
			Bat.MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);
			Bat.MID->SetScalarParameterValue(TEXT("Opacity"), 1.f);
			Bat.Mesh = AddLiteQuad(Bat.MID);
		}
	}
}

void AHalloweenProps::TickLite(float DeltaTime)
{
	RefreshLiteFrame();
	AdaptResolution(DeltaTime);

	for (FLiteSteam& Puff : LiteSteam)
	{
		Puff.Age += DeltaTime;
		if (Puff.Age > Puff.Life)
		{
			Puff.Age -= Puff.Life;
			Puff.Drift = FMath::FRandRange(-0.05f, 0.05f);
		}
		const float U = Puff.Age / Puff.Life;
		const FVector2D At = Puff.Anchor + FVector2D(Puff.Drift * U + 0.015f * FMath::Sin(Time * 0.9f + Puff.Anchor.X * 5.f + Puff.Life), 0.04f + 0.4f * U);
		PlaceOnBackdrop(Puff.Mesh, At, SteamDepth * LiteDepthScale, Puff.Size * (0.5f + 0.9f * U), 1.f, false);
		Puff.MID->SetScalarParameterValue(TEXT("Frame"), FMath::FloorToFloat(Puff.Age * 6.f + Puff.Life * 3.f));
		Puff.MID->SetScalarParameterValue(TEXT("Opacity"), 0.45f * FMath::Sin(U * PI));
	}

	for (FLiteBat& Bat : LiteBats)
	{
		const float Angle = Time * Bat.Speed + Bat.Phase;
		FVector2D At;
		float Heading;
		if (Bat.bCrossing)
		{
			At = Bat.Centre + FVector2D(Bat.Radius.X * FMath::Sin(Angle * 2.f), Bat.Radius.Y * FMath::Sin(Time * 1.7f + Bat.Phase));
			Heading = FMath::Cos(Angle * 2.f);
		}
		else
		{
			At = Bat.Centre + FVector2D(Bat.Radius.X * FMath::Cos(Angle), Bat.Radius.Y * FMath::Sin(Time * 1.3f + Bat.Phase) + 0.4f * Bat.Radius.Y * FMath::Sin(Angle));
			Heading = -FMath::Sin(Angle) * FMath::Sign(Bat.Speed);
		}
		if (FMath::Abs(Heading) > 0.1f)
		{
			Bat.bFacingRight = Heading > 0.f;
		}
		PlaceOnBackdrop(Bat.Mesh, At, BatDepth * LiteDepthScale, Bat.Size, 1.f, false);
		Bat.MID->SetScalarParameterValue(TEXT("Frame"), FMath::FloorToFloat(Time * Bat.FlapRate + Bat.Phase * 3.f));
	}
}

void AHalloweenProps::SetLiteLayerHidden(const FString& Layer, bool bHide)
{
	if (Layer == TEXT("backdrop") && LiteBackdrop)
	{
		LiteBackdrop->SetVisibility(!bHide);
	}
	if (Layer == TEXT("bats"))
	{
		for (FLiteBat& Bat : LiteBats)
		{
			Bat.Mesh->SetVisibility(!bHide);
		}
	}
	if (Layer == TEXT("steam"))
	{
		for (FLiteSteam& Puff : LiteSteam)
		{
			Puff.Mesh->SetVisibility(!bHide);
		}
	}
}

void AHalloweenProps::SetCaptureMode()
{
	// For rendering the backdrop picture: no sprites and no 3D animation, only the static scene.
	for (FLiteBat& Bat : LiteBats)
	{
		if (Bat.Mesh)
		{
			Bat.Mesh->SetVisibility(false);
		}
	}
	for (FLiteSteam& Puff : LiteSteam)
	{
		if (Puff.Mesh)
		{
			Puff.Mesh->SetVisibility(false);
		}
	}
	for (FBat& Bat : Bats)
	{
		if (Bat.Mesh)
		{
			Bat.Mesh->SetVisibility(false);
		}
	}
	for (FPuff& Puff : Puffs)
	{
		if (Puff.Mesh)
		{
			Puff.Mesh->SetVisibility(false);
		}
	}
	bCapture = true;
}
