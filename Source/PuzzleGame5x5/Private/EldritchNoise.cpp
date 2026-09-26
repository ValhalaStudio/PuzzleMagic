#include "EldritchNoise.h"

#if WITH_FASTNOISE2
THIRD_PARTY_INCLUDES_START
#include "FastNoise/FastNoise.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace EldritchNoise
{
#if WITH_FASTNOISE2
	namespace
	{
		FastNoise::SmartNode<FastNoise::FractalFBm> MakeFractal(float FeatureSize)
		{
			auto Simplex = FastNoise::New<FastNoise::Simplex>();
			Simplex->SetScale(FeatureSize);
			auto Fractal = FastNoise::New<FastNoise::FractalFBm>();
			Fractal->SetSource(Simplex);
			Fractal->SetOctaveCount(4);
			Fractal->SetGain(0.5f);
			return Fractal;
		}
	}
#endif

	void Fractal3D(TConstArrayView<FVector3f> Points, float FeatureSize, int32 Seed, TArray<float>& Out)
	{
		const int32 Count = Points.Num();
		Out.SetNumUninitialized(Count);
		if (Count == 0)
		{
			return;
		}
#if WITH_FASTNOISE2
		// Structure-of-arrays positions for the SIMD batch call.
		TArray<float> Xs, Ys, Zs;
		Xs.SetNumUninitialized(Count);
		Ys.SetNumUninitialized(Count);
		Zs.SetNumUninitialized(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Xs[Index] = Points[Index].X;
			Ys[Index] = Points[Index].Y;
			Zs[Index] = Points[Index].Z;
		}
		MakeFractal(FeatureSize)->GenPositionArray3D(Out.GetData(), Count, Xs.GetData(), Ys.GetData(), Zs.GetData(), 0.f, 0.f, 0.f, Seed);
#else
		const FVector Offset(Seed * 17.31, Seed * 5.77, Seed * 11.13);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector P = FVector(Points[Index]) / FMath::Max(FeatureSize, 1.f) + Offset;
			float Sum = 0.f;
			float Amp = 0.5f;
			float Freq = 1.f;
			for (int32 Octave = 0; Octave < 4; ++Octave)
			{
				Sum += Amp * FMath::PerlinNoise3D(P * Freq);
				Amp *= 0.5f;
				Freq *= 2.f;
			}
			Out[Index] = Sum * 2.f;
		}
#endif
	}

	float Fractal1D(float T, int32 Seed)
	{
#if WITH_FASTNOISE2
		static const auto Fractal = MakeFractal(1.f);
		return Fractal->GenSingle2D(T, 0.f, Seed);
#else
		return FMath::PerlinNoise1D(T + Seed * 7.31f) * 1.6f;
#endif
	}
}
