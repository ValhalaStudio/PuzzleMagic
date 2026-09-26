#include "MeshBuffersUtil.h"
#include "RealtimeMeshSimple.h"
#include "Core/RealtimeMeshBuilder.h"
#include "meshoptimizer.h"
#include "UObject/Package.h"

namespace MeshBuffers
{
	void Optimize(ToonMesh::FBuffers& B)
	{
		const size_t VertexCount = B.Vertices.Num();
		const size_t IndexCount = B.Triangles.Num();
		if (VertexCount == 0 || IndexCount == 0)
		{
			return;
		}
		static_assert(sizeof(int32) == sizeof(unsigned int), "index layout");
		unsigned int* Indices = reinterpret_cast<unsigned int*>(B.Triangles.GetData());
		meshopt_optimizeVertexCache(Indices, Indices, IndexCount, VertexCount);

		TArray<unsigned int> Remap;
		Remap.SetNumUninitialized(VertexCount);
		const size_t Unique = meshopt_optimizeVertexFetchRemap(Remap.GetData(), Indices, IndexCount, VertexCount);
		meshopt_remapIndexBuffer(Indices, Indices, IndexCount, Remap.GetData());
		auto Apply = [&](auto& Array)
		{
			using FElem = typename TRemoveReference<decltype(Array)>::Type::ElementType;
			if (Array.Num() != static_cast<int32>(VertexCount))
			{
				return;
			}
			TArray<FElem> Out;
			Out.SetNum(Unique);
			for (size_t Old = 0; Old < VertexCount; ++Old)
			{
				if (Remap[Old] != ~0u)
				{
					Out[Remap[Old]] = Array[Old];
				}
			}
			Array = MoveTemp(Out);
		};
		Apply(B.Vertices);
		Apply(B.Normals);
		Apply(B.UVs);
		Apply(B.Colors);
	}

	URealtimeMeshSimple* BuildRealtimeMesh(UObject* Outer, TArrayView<const ToonMesh::FBuffers* const> Parts)
	{
		using namespace RealtimeMesh;
		URealtimeMeshSimple* Mesh = NewObject<URealtimeMeshSimple>(Outer);

		FRealtimeMeshStreamSet StreamSet;
		TRealtimeMeshBuilderLocal<uint32, FPackedNormal, FVector2DHalf, 1> Builder(StreamSet);
		Builder.EnableTangents();
		Builder.EnableTexCoords();
		Builder.EnableColors();
		Builder.EnablePolyGroups();

		for (int32 Part = 0; Part < Parts.Num(); ++Part)
		{
			const ToonMesh::FBuffers& B = *Parts[Part];
			Mesh->SetupMaterialSlot(Part, FName(*FString::Printf(TEXT("Part%d"), Part)));
			const int32 Base = Builder.NumVertices();
			for (int32 Index = 0; Index < B.Vertices.Num(); ++Index)
			{
				const FVector3f Normal = B.Normals.IsValidIndex(Index) ? FVector3f(B.Normals[Index]) : FVector3f::UpVector;
				// Any tangent perpendicular to the normal will do: these materials use no normal maps.
				const FVector3f Helper = FMath::Abs(Normal.Z) < 0.9f ? FVector3f::UpVector : FVector3f::ForwardVector;
				const FVector3f Tangent = FVector3f::CrossProduct(Helper, Normal).GetSafeNormal();
				Builder.AddVertex(FVector3f(B.Vertices[Index]))
					.SetNormalAndTangent(Normal, Tangent)
					.SetTexCoord(B.UVs.IsValidIndex(Index) ? FVector2f(B.UVs[Index]) : FVector2f::ZeroVector)
					.SetColor(B.Colors.IsValidIndex(Index) ? B.Colors[Index] : FLinearColor::White, false);
			}
			for (int32 Tri = 0; Tri + 2 < B.Triangles.Num(); Tri += 3)
			{
				Builder.AddTriangle(Base + B.Triangles[Tri], Base + B.Triangles[Tri + 1], Base + B.Triangles[Tri + 2], Part);
			}
		}

		const FRealtimeMeshBufferSetKey Key = FRealtimeMeshBufferSetKey::Create(0, FName("Parts"));
		Mesh->CreateBufferSet(Key, MoveTemp(StreamSet));
		for (int32 Part = 0; Part < Parts.Num(); ++Part)
		{
			Mesh->UpdateSectionConfig(FRealtimeMeshSectionKey::CreateForPolyGroup(Key, Part), FRealtimeMeshSectionConfig(Part));
		}
		return Mesh;
	}

	URealtimeMeshSimple* GetSharedMesh(FName Key, TFunctionRef<TArray<ToonMesh::FBuffers>()> Build)
	{
		static TMap<FName, TWeakObjectPtr<URealtimeMeshSimple>> Cache;
		if (URealtimeMeshSimple* Existing = Cache.FindRef(Key).Get())
		{
			return Existing;
		}
		TArray<ToonMesh::FBuffers> Parts = Build();
		TArray<const ToonMesh::FBuffers*> Views;
		for (ToonMesh::FBuffers& Part : Parts)
		{
			Optimize(Part);
			Views.Add(&Part);
		}
		// Outered to the transient package and rooted: it lives as long as the process, shared by every user.
		URealtimeMeshSimple* Mesh = BuildRealtimeMesh(GetTransientPackage(), Views);
		Mesh->AddToRoot();
		Cache.Add(Key, Mesh);
		return Mesh;
	}
}
