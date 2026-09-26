#pragma once

#include "CoreMinimal.h"
#include "ToonMeshBuilder.h"

class URealtimeMeshSimple;
class URealtimeMeshComponent;

namespace MeshBuffers
{
	// Reorders a mesh for the GPU with meshoptimizer: triangles for post-transform vertex cache
	// hits, then vertices in first-use order. Geometry and look are unchanged.
	void Optimize(ToonMesh::FBuffers& Buffers);

	// Builds a URealtimeMeshSimple holding Parts as one buffer set, part i = poly group i =
	// material slot i. The mesh is outered to (and kept alive by) Outer; one mesh can be given to
	// any number of components with URealtimeMeshComponent::SetRealtimeMesh (shared GPU buffers).
	URealtimeMeshSimple* BuildRealtimeMesh(UObject* Outer, TArrayView<const ToonMesh::FBuffers* const> Parts);

	// A mesh built once per process and shared by every caller with the same Key (e.g. all tiles).
	URealtimeMeshSimple* GetSharedMesh(FName Key, TFunctionRef<TArray<ToonMesh::FBuffers>()> Build);
}
