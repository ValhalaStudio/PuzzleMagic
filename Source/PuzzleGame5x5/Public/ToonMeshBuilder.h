#pragma once

#include "CoreMinimal.h"

// Builds rounded, bevelled rectangular blocks for the cartoon look, as raw
// buffers that MeshBuffers turns into (shared) realtime meshes.
namespace ToonMesh
{
	struct FBuffers
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;

		void Append(const FBuffers& Other, const FVector& Offset);
	};

	struct FBlockParams
	{
		FVector2D HalfExtent = FVector2D(40.f, 40.f);
		float CornerRadius = 14.f;
		float Height = 26.f;
		float Bevel = 7.f;
		int32 CornerSegments = 5;
		int32 BevelSegments = 4;
	};

	// Block sitting on z = 0. The top cap is flagged with vertex colour R = 1 and
	// carries 0..1 UVs across its face; walls and bevel have R = 0.
	FBuffers BuildBlock(const FBlockParams& Params);

	// Same block grown by Thickness along its surface normals with reversed
	// winding: rendered one-sided, only its far side shows, as an ink outline.
	FBuffers BuildOutlineHull(const FBlockParams& Params, float Thickness);
}
