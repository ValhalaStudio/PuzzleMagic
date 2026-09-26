#include "PuzzleCameraPawn.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APuzzleCameraPawn::APuzzleCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	// The framing maths treats FieldOfView as horizontal; pin that rather than rely on project defaults.
	Camera->bOverrideAspectRatioAxisConstraint = true;
	Camera->AspectRatioAxisConstraint = EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV;

	// APawn defaults to following the controller's yaw every frame, which would
	// overwrite the framing below with the PlayerStart's yaw.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	AutoPossessPlayer = EAutoReceiveInput::Player0;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InkFinder(TEXT("/Game/Materials/M_PPInkOutline.M_PPInkOutline"));
	InkOutlineMaterial = InkFinder.Object;
}

void APuzzleCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = false;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = ExposureCompensation;
	// The filmic tone curve desaturates bright flat colours; cartoon shading wants them as authored.
	PP.bOverride_ToneCurveAmount = true;
	PP.ToneCurveAmount = 0.f;
	// Tumbling tiles smear into blurs otherwise; crisp motion suits the cartoon look.
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.f;

	// Storybook grade: richer colour, a touch more contrast, warm highlights over cool moonlit shadows.
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(1.15f, 1.15f, 1.15f, 1.f);
	PP.bOverride_ColorContrast = true;
	PP.ColorContrast = FVector4(1.08f, 1.08f, 1.08f, 1.f);
	PP.bOverride_ColorGainHighlights = true;
	PP.ColorGainHighlights = FVector4(1.04f, 1.f, 0.94f, 1.f);
	PP.bOverride_ColorGainShadows = true;
	PP.ColorGainShadows = FVector4(0.94f, 0.96f, 1.08f, 1.f);

	// Old-film horror texture: grain, a whisper of lens fringing, heavy corners.
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = 0.22f;
	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = 0.6f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.75f;

	// Lens flares bloom off lightning, candles and holy light; faint enough to stay out of the way otherwise.
	PP.bOverride_LensFlareIntensity = true;
	PP.LensFlareIntensity = 0.35f;
	PP.bOverride_LensFlareThreshold = true;
	PP.LensFlareThreshold = 6.f;

	// Cartoon ink lines wherever depth or normals break (tile edges, bezels, the cathedral).
	if (InkOutlineMaterial)
	{
		PP.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, UMaterialInstanceDynamic::Create(InkOutlineMaterial, this)));
	}
	Camera->PostProcessBlendWeight = 1.f;

	UpdateFraming();
}

void APuzzleCameraPawn::SetFramingBounds(const FBox2D& Bounds, float GroundZ)
{
	FramingBounds = Bounds;
	FramingGroundZ = GroundZ;
	UpdateFraming();
}

void APuzzleCameraPawn::AddShake(float Strength)
{
	ShakeAmplitude = FMath::Max(ShakeAmplitude, Strength);
}

void APuzzleCameraPawn::UpdateFraming()
{
	FVector2D ViewportSize(16.f, 9.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	if (ViewportSize.X <= 0.f || ViewportSize.Y <= 0.f)
	{
		return;
	}
	LastViewportSize = ViewportSize;

	const float Aspect = ViewportSize.X / ViewportSize.Y;
	const float HorizontalFOV = Aspect < 1.f ? PortraitFOV : LandscapeFOV;
	Camera->SetFieldOfView(HorizontalFOV);

	// UE keeps the horizontal FOV fixed, so the vertical one follows the aspect ratio.
	const float TanHalfH = FMath::Tan(FMath::DegreesToRadians(HorizontalFOV * 0.5f));
	const float TanHalfV = TanHalfH / Aspect;

	const FVector2D Center = FramingBounds.GetCenter();
	const FVector2D HalfSize = FramingBounds.GetExtent();
	const float Tilt = FMath::Sin(FMath::DegreesToRadians(-ViewPitch));
	const float Distance = FMath::Max(HalfSize.X / TanHalfH, HalfSize.Y * Tilt / TanHalfV) * FramingMargin;

	const FRotator ViewRotation(ViewPitch, ViewYaw, 0.f);
	const FVector Focus(Center.X, Center.Y + FocusShiftY, FramingGroundZ);
	RestLocation = Focus - ViewRotation.Vector() * Distance;
	SetActorLocationAndRotation(RestLocation, ViewRotation);
}

void APuzzleCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		if (!ViewportSize.Equals(LastViewportSize, 0.5f))
		{
			UpdateFraming();
		}
	}

	if (ShakeAmplitude > 0.05f)
	{
		const FRotator Rotation = GetActorRotation();
		const FVector Offset = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * FMath::FRandRange(-1.f, 1.f)
			+ FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * FMath::FRandRange(-1.f, 1.f);
		SetActorLocation(RestLocation + Offset * ShakeAmplitude);
		ShakeAmplitude *= FMath::Exp(-7.f * DeltaTime);
	}
	else if (ShakeAmplitude > 0.f)
	{
		ShakeAmplitude = 0.f;
		SetActorLocation(RestLocation);
	}
}
