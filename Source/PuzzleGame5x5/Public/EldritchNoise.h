#pragma once

#include "CoreMinimal.h"

// Organic noise for procedural meshes and light behaviour. On Win64 it runs FastNoise2
// (SIMD fractal simplex, dispatched to SSE2..AVX-512); elsewhere (iOS) FMath::PerlinNoise3D.
namespace EldritchNoise
{
	// Fractal noise (roughly -1..1) at each point; FeatureSize is the size of the largest features.
	void Fractal3D(TConstArrayView<FVector3f> Points, float FeatureSize, int32 Seed, TArray<float>& Out);

	// Smooth 1D fractal noise (roughly -1..1), e.g. to make a light's stutter irregular.
	float Fractal1D(float T, int32 Seed);
}
