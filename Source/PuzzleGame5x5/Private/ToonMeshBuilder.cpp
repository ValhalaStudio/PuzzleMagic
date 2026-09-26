#include "ToonMeshBuilder.h"

namespace ToonMesh
{
	void FBuffers::Append(const FBuffers& Other, const FVector& Offset)
	{
		const int32 Base = Vertices.Num();
		for (const FVector& V : Other.Vertices)
		{
			Vertices.Add(V + Offset);
		}
		for (int32 Index : Other.Triangles)
		{
			Triangles.Add(Base + Index);
		}
		Normals.Append(Other.Normals);
		UVs.Append(Other.UVs);
		Colors.Append(Other.Colors);
	}

	namespace
	{
		// Rounded-rectangle outline, counter-clockwise seen from +Z, pushed outward by Offset.
		void Ring(const FBlockParams& P, float Offset, TArray<FVector2D>& OutPoints, TArray<FVector2D>& OutDirs)
		{
			const float CX = P.HalfExtent.X - P.CornerRadius;
			const float CY = P.HalfExtent.Y - P.CornerRadius;
			const FVector2D Centers[4] = { { CX, CY }, { -CX, CY }, { -CX, -CY }, { CX, -CY } };
			const float Radius = FMath::Max(P.CornerRadius + Offset, 0.f);

			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				for (int32 Step = 0; Step <= P.CornerSegments; ++Step)
				{
					const float Angle = FMath::DegreesToRadians(90.f * Corner + 90.f * Step / P.CornerSegments);
					const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
					OutPoints.Add(Centers[Corner] + Dir * Radius);
					OutDirs.Add(Dir);
				}
			}
		}

		FBuffers Build(const FBlockParams& P, float Expand, bool bFlipWinding)
		{
			FBuffers Out;
			TArray<FVector2D> Dirs;

			struct FRingSpec { float Offset; float Z; float CosT; float SinT; };
			TArray<FRingSpec> Rings;
			Rings.Add({ Expand, -Expand, 1.f, 0.f }); // bottom of the wall
			for (int32 Step = 0; Step <= P.BevelSegments; ++Step)
			{
				// Quarter-circle bevel: arc centred at inset Bevel, height Height - Bevel.
				const float T = FMath::DegreesToRadians(90.f * Step / P.BevelSegments);
				const float Radius = P.Bevel + Expand;
				Rings.Add({ -P.Bevel + Radius * FMath::Cos(T), P.Height - P.Bevel + Radius * FMath::Sin(T), FMath::Cos(T), FMath::Sin(T) });
			}

			int32 RingSize = 0;
			for (const FRingSpec& Spec : Rings)
			{
				TArray<FVector2D> Points;
				Dirs.Reset();
				Ring(P, Spec.Offset, Points, Dirs);
				RingSize = Points.Num();
				for (int32 I = 0; I < Points.Num(); ++I)
				{
					Out.Vertices.Add(FVector(Points[I], Spec.Z));
					Out.Normals.Add(FVector(Dirs[I] * Spec.CosT, Spec.SinT).GetSafeNormal());
					Out.UVs.Add(FVector2D::ZeroVector);
					Out.Colors.Add(FLinearColor(0.f, 0.f, 0.f, 1.f));
				}
			}

			auto AddTri = [&Out, bFlipWinding](int32 A, int32 B, int32 C)
			{
				Out.Triangles.Add(A);
				Out.Triangles.Add(bFlipWinding ? C : B);
				Out.Triangles.Add(bFlipWinding ? B : C);
			};

			for (int32 R = 0; R + 1 < Rings.Num(); ++R)
			{
				const int32 Lower = R * RingSize;
				const int32 Upper = (R + 1) * RingSize;
				for (int32 I = 0; I < RingSize; ++I)
				{
					const int32 J = (I + 1) % RingSize;
					AddTri(Lower + I, Lower + J, Upper + I);
					AddTri(Lower + J, Upper + J, Upper + I);
				}
			}

			// Caps get their own vertices so the flat normals and the top-face flag don't bleed into the bevel.
			const float CapHalfX = FMath::Max(P.HalfExtent.X - P.Bevel, 1.f);
			const float CapHalfY = FMath::Max(P.HalfExtent.Y - P.Bevel, 1.f);
			auto AddCap = [&](int32 SourceRing, float NormalZ, const FLinearColor& Color, bool bTop)
			{
				const float Z = Out.Vertices[SourceRing * RingSize].Z;
				const int32 Center = Out.Vertices.Num();
				Out.Vertices.Add(FVector(0.f, 0.f, Z));
				Out.Normals.Add(FVector(0.f, 0.f, NormalZ));
				Out.UVs.Add(FVector2D(0.5f, 0.5f));
				Out.Colors.Add(Color);

				const int32 First = Out.Vertices.Num();
				for (int32 I = 0; I < RingSize; ++I)
				{
					const FVector V = Out.Vertices[SourceRing * RingSize + I];
					Out.Vertices.Add(V);
					Out.Normals.Add(FVector(0.f, 0.f, NormalZ));
					Out.UVs.Add(FVector2D(0.5f + V.X / (2.f * CapHalfX), 0.5f + V.Y / (2.f * CapHalfY)));
					Out.Colors.Add(Color);
				}
				for (int32 I = 0; I < RingSize; ++I)
				{
					const int32 J = (I + 1) % RingSize;
					if (bTop) { AddTri(Center, First + I, First + J); }
					else      { AddTri(Center, First + J, First + I); }
				}
			};
			AddCap(Rings.Num() - 1, 1.f, FLinearColor(1.f, 0.f, 0.f, 1.f), true);
			AddCap(0, -1.f, FLinearColor(0.f, 0.f, 0.f, 1.f), false);

			return Out;
		}
	}

	// Build() emits triangles whose (b-a)x(c-a) points outward; Unreal (left-handed)
	// treats that order as back-facing, so the solid block needs it flipped and the
	// inside-out outline hull keeps it.
	FBuffers BuildBlock(const FBlockParams& Params)
	{
		return Build(Params, 0.f, true);
	}

	FBuffers BuildOutlineHull(const FBlockParams& Params, float Thickness)
	{
		// Only the hull's inner side is ever seen, so its normals face inward for lit (bezel) materials.
		FBuffers Hull = Build(Params, Thickness, false);
		for (FVector& Normal : Hull.Normals)
		{
			Normal = -Normal;
		}
		return Hull;
	}
}
