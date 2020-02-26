// Copyright 2020 Phyronnaz

#include "makelevelset3.h"
#include "VoxelIntVectorUtilities.h"

// 10% faster to build with FORCEINLINE
FORCEINLINE float PointSegmentDistance(const FVector& Point, const FVector& A, const FVector& B, float& Alpha)
{
	const FVector AB = B - A;
	const float Size = AB.SizeSquared();
	// Find parameter value of closest point on segment
	Alpha = FVector::DotProduct(B - Point, AB) / Size;
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
	// And find the distance
	return FVector::Dist(Point, FMath::Lerp(B, A, Alpha));
}

// 10-20% better perf with this FORCEINLINE
FORCEINLINE float PointTriangleDistance(
	const FVector& Point,
	const FVector& A,
	const FVector& B,
	const FVector& C,
	float& AlphaA, float& AlphaB, float& AlphaC)
{
	// First find barycentric coordinates of closest point on infinite plane
	{
		const FVector CA = A - C;
		const FVector CB = B - C;
		const FVector CPoint = Point - C;
		const float SizeCA = CA.SizeSquared();
		const float SizeCB = CB.SizeSquared();
		const float d = FVector::DotProduct(CA, CB);
		const float InvDet = 1.f / FMath::Max(SizeCA * SizeCB - d * d, SMALL_NUMBER);
		const float a = FVector::DotProduct(CA, CPoint);
		const float b = FVector::DotProduct(CB, CPoint);

		// The barycentric coordinates themselves
		AlphaA = InvDet * (SizeCB * a - d * b);
		AlphaB = InvDet * (SizeCA * b - d * a);
		AlphaC = 1 - AlphaA - AlphaB;
	}

	if (AlphaA >= 0 && AlphaB >= 0 && AlphaC >= 0)
	{
		// If we're inside the triangle
		return FVector::Dist(Point, AlphaA * A + AlphaB * B + AlphaC * C);
	}
	else
	{
		// We have to clamp to one of the edges

		if (AlphaA > 0)
		{
			// This rules out edge BC for us
			float AlphaAB;
			const float DistanceAB = PointSegmentDistance(Point, A, B, AlphaAB);
			float AlphaAC;
			const float DistanceAC = PointSegmentDistance(Point, A, C, AlphaAC);

			if (DistanceAB < DistanceAC)
			{
				AlphaA = AlphaAB;
				AlphaB = 1 - AlphaAB;
				AlphaC = 0;
				return DistanceAB;
			}
			else
			{
				AlphaA = AlphaAC;
				AlphaB = 0;
				AlphaC = 1 - AlphaAC;
				return DistanceAC;
			}
		}
		else if (AlphaB > 0)
		{
			// This rules out edge AC
			float AlphaAB;
			const float DistanceAB = PointSegmentDistance(Point, A, B, AlphaAB);
			float AlphaBC;
			const float DistanceBC = PointSegmentDistance(Point, B, C, AlphaBC);

			if (DistanceAB < DistanceBC)
			{
				AlphaA = AlphaAB;
				AlphaB = 1 - AlphaAB;
				AlphaC = 0;
				return DistanceAB;
			}
			else
			{
				AlphaA = 0;
				AlphaB = AlphaBC;
				AlphaC = 1 - AlphaBC;
				return DistanceBC;
			}
		}
		else
		{
			ensureVoxelSlow(AlphaC > 0);
			// Rules out edge AB

			float AlphaBC;
			const float DistanceBC = PointSegmentDistance(Point, B, C, AlphaBC);
			float AlphaAC;
			const float DistanceAC = PointSegmentDistance(Point, A, C, AlphaAC);

			if (DistanceBC < DistanceAC)
			{
				AlphaA = 0;
				AlphaB = AlphaBC;
				AlphaC = 1 - AlphaBC;
				return DistanceBC;
			}
			else
			{
				AlphaA = AlphaAC;
				AlphaB = 0;
				AlphaC = 1 - AlphaAC;
				return DistanceAC;
			}
		}
	}
}

inline void CheckNeighbor(
	const TFastArrayView<FIntVector>& Triangles,
	const TFastArrayView<FVector>& Vertices,
	TArray3<float>& Phi,
	TArray3<int32>& ClosestTriangleIndices,
	const FVector& Position,
	const FIntVector& PositionA,
	const FIntVector& PositionB)
{
	const int32 ClosestTriangleIndex = ClosestTriangleIndices(PositionB);
	if (ClosestTriangleIndex < 0) return;

	const FIntVector ClosestTriangle = Triangles[ClosestTriangleIndex];

	float AlphaA, AlphaB, AlphaC;
	const float Distance = PointTriangleDistance(
		Position,
		Vertices[ClosestTriangle.X], Vertices[ClosestTriangle.Y], Vertices[ClosestTriangle.Z],
		AlphaA, AlphaB, AlphaC);

	if (Distance < Phi(PositionA))
	{
		Phi(PositionA) = Distance;
		ClosestTriangleIndices(PositionA) = ClosestTriangleIndex;
	}
}

inline void Sweep(
	const TFastArrayView<FIntVector>& Triangles,
	const TFastArrayView<FVector>& Vertices,
	TArray3<float>& Phi,
	TArray3<int32>& ClosestTriangleIndices,
	const FVector& Origin,
	float Delta,
	const FIntVector& DeltaIt)
{
	VOXEL_FUNCTION_COUNTER();

	FIntVector Start;
	FIntVector End;
	for (int32 Index = 0; Index < 3; Index++)
	{
		if (DeltaIt[Index] > 0)
		{
			Start[Index] = 1;
			End[Index] = Phi.Size[Index];
		}
		else
		{
			Start[Index] = Phi.Size[Index] - 2;
			End[Index] = -1;
		}
	}

	for (int32 Z = Start.Z; Z != End.Z; Z += DeltaIt.Z)
	{
		for (int32 Y = Start.Y; Y != End.Y; Y += DeltaIt.Y)
		{
			for (int32 X = Start.X; X != End.X; X += DeltaIt.X)
			{
				const FVector Position = FVector(X, Y, Z) * Delta + Origin;
				const FIntVector PositionA = FIntVector(X, Y, Z);
				const auto Check = [&](int32 OffsetX, int32 OffsetY, int32 OffsetZ)
				{
					const FIntVector PositionB = PositionA + DeltaIt * FIntVector(OffsetX, OffsetY, OffsetZ);
					CheckNeighbor(Triangles, Vertices, Phi, ClosestTriangleIndices, Position, PositionA, PositionB);
				};
				Check(-1, -0, -0);
				Check(-0, -1, -0);
				Check(-1, -1, -0);
				Check(-0, -0, -1);
				Check(-1, -0, -1);
				Check(-0, -1, -1);
				Check(-1, -1, -1);
			}
		}
	}
}

struct FVector2D_Double
{
	double X;
	double Y;

	FVector2D_Double() = default;
	FVector2D_Double(double X, double Y)
		: X(X)
		, Y(Y)
	{
	}

	inline FVector2D_Double operator*(const FVector2D_Double& Other) const
	{
		return { X * Other.X, Y * Other.Y };
	}
	inline FVector2D_Double operator+(const FVector2D_Double& Other) const
	{
		return { X + Other.X, Y + Other.Y };
	}
	inline FVector2D_Double operator-(const FVector2D_Double& Other) const
	{
		return { X - Other.X, Y - Other.Y };
	}
	inline FVector2D_Double& operator-=(const FVector2D_Double& Other)
	{
		return *this = *this - Other;
	}
};

// Calculate twice signed area of triangle (0,0)-(A.X,A.Y)-(B.X,B.Y)
// Return an SOS-determined sign (-1, +1, or 0 only if it's a truly degenerate triangle)
inline int32 Orientation(
	const FVector2D_Double& A,
	const FVector2D_Double& B,
	double& TwiceSignedArea)
{
	TwiceSignedArea = A.Y * B.X - A.X * B.Y;
	if (TwiceSignedArea > 0) return 1;
	else if (TwiceSignedArea < 0) return -1;
	else if (B.Y > A.Y) return 1;
	else if (B.Y < A.Y) return -1;
	else if (A.X > B.X) return 1;
	else if (A.X < B.X) return -1;
	else return 0; // Only true when A.X == B.X and A.Y == B.Y
}

// Robust test of (x0,y0) in the triangle (x1,y1)-(x2,y2)-(x3,y3)
// If true is returned, the barycentric coordinates are set in a,b,c.
inline bool PointInTriangle2D(
	const FVector2D_Double& Point,
	FVector2D_Double A,
	FVector2D_Double B,
	FVector2D_Double C,
	double& AlphaA, double& AlphaB, double& AlphaC)
{
	A -= Point;
	B -= Point;
	C -= Point;

	const int32 SignA = Orientation(B, C, AlphaA);
	if (SignA == 0) return false;

	const int32 SignB = Orientation(C, A, AlphaB);
	if (SignB != SignA) return false;

	const int32 SignC = Orientation(A, B, AlphaC);
	if (SignC != SignA) return false;

	const double Sum = AlphaA + AlphaB + AlphaC;
	checkSlow(Sum != 0); // if the SOS signs match and are non-zero, there's no way all of a, b, and c are zero.
	AlphaA /= Sum;
	AlphaB /= Sum;
	AlphaC /= Sum;

	return true;
}

void MakeLevelSet3(
	const FMakeLevelSet3Settings& Settings,
	TArray3<float>& OutPhi,
	TArray3<FVector2D>& OutUVs,
	TArray<TArray3<FColor>>& OutColors,
	int32& OutNumLeaks)
{
	VOXEL_FUNCTION_COUNTER();

	check(!Settings.bExportUVs || Settings.UVs.Num() == Settings.Vertices.Num());
	check(Settings.ColorTextures.Num() == 0 || Settings.UVs.Num() == Settings.Vertices.Num());

	const FIntVector Size = Settings.Size;

	OutPhi.Resize(Size);
	OutPhi.Assign(MAX_flt);

	if (Settings.bExportUVs)
	{
		OutUVs.Resize(Size);
		OutUVs.Memzero();
	}
	OutColors.SetNum(Settings.ColorTextures.Num());
	for (int32 Index = 0; Index < Settings.ColorTextures.Num(); Index++)
	{
		OutColors[Index].Resize(Size);
		OutColors[Index].Memzero();
	}
	TArray3<bool> Computed(Size);
	Computed.Memzero();

	TArray3<int32> ClosestTriangleIndices(Size);
	ClosestTriangleIndices.Assign(-1);

	TArray3<int32> IntersectionCount(Size); // IntersectionCount(i,j,k) is # of tri intersections in (i-1,i]x{j}x{k}
	IntersectionCount.Assign(0);

	// We begin by initializing distances near the mesh, and figuring out intersection counts
	{
		VOXEL_SCOPE_COUNTER("Intersections");
		for (int32 TriangleIndex = 0; TriangleIndex < Settings.Triangles.Num(); TriangleIndex++)
		{
			VOXEL_SLOW_SCOPE_COUNTER("Process triangle");

			const FIntVector Triangle = Settings.Triangles[TriangleIndex];
			const int32 IndexA = Triangle.X;
			const int32 IndexB = Triangle.Y;
			const int32 IndexC = Triangle.Z;

			const FVector VertexA = Settings.Vertices[IndexA];
			const FVector VertexB = Settings.Vertices[IndexB];
			const FVector VertexC = Settings.Vertices[IndexC];

			const auto ToVoxelSpace = [&](const FVector& Value)
			{
				return (Value - Settings.Origin) / Settings.Delta;
			};
			const auto FromVoxelSpace = [&](const FVector& Value)
			{
				return Value * Settings.Delta + Settings.Origin;
			};

			const FVector VoxelVertexA = ToVoxelSpace(VertexA);
			const FVector VoxelVertexB = ToVoxelSpace(VertexB);
			const FVector VoxelVertexC = ToVoxelSpace(VertexC);

			const FVector MinVoxelVertex = FVoxelUtilities::ComponentMin3(VoxelVertexA, VoxelVertexB, VoxelVertexC);
			const FVector MaxVoxelVertex = FVoxelUtilities::ComponentMax3(VoxelVertexA, VoxelVertexB, VoxelVertexC);

			{
				VOXEL_SLOW_SCOPE_COUNTER("Compute distance");

				const FIntVector Start = FVoxelUtilities::Clamp(FIntVector(MinVoxelVertex) - Settings.ExactBand, FIntVector(0), Size - 1);
				const FIntVector End = FVoxelUtilities::Clamp(FIntVector(MaxVoxelVertex) + Settings.ExactBand + 1, FIntVector(0), Size - 1);

				// Do distances nearby
				for (int32 Z = Start.Z; Z <= End.Z; Z++)
				{
					for (int32 Y = Start.Y; Y <= End.Y; Y++)
					{
						for (int32 X = Start.X; X <= End.X; X++)
						{
							const FVector Position = FromVoxelSpace(FVector(X, Y, Z));
							float AlphaA, AlphaB, AlphaC;
							const float Distance = PointTriangleDistance(Position, VertexA, VertexB, VertexC, AlphaA, AlphaB, AlphaC);
							if (Distance < OutPhi(X, Y, Z))
							{
								Computed(X, Y, Z) = true;
								OutPhi(X, Y, Z) = Distance;
								ClosestTriangleIndices(X, Y, Z) = TriangleIndex;
								if (Settings.bExportUVs || Settings.ColorTextures.Num() > 0)
								{
									const FVector2D UV = AlphaA * Settings.UVs[IndexA] + AlphaB * Settings.UVs[IndexB] + AlphaC * Settings.UVs[IndexC];
									if (Settings.bExportUVs)
									{
										OutUVs(X, Y, Z) = UV;
									}
									for (int32 Index = 0; Index < Settings.ColorTextures.Num(); Index++)
									{
										const auto& ColorTexture = Settings.ColorTextures[Index];
										OutColors[Index](X, Y, Z) = ColorTexture.Sample<FLinearColor>(
											UV.X * ColorTexture.GetSizeX(),
											UV.Y * ColorTexture.GetSizeY(),
											EVoxelSamplerMode::Tile).ToFColor(true);
									}
								}
							}
						}
					}
				}
			}

			{
				VOXEL_SLOW_SCOPE_COUNTER("Compute intersections");

				const FIntVector Start = FVoxelUtilities::Clamp(FVoxelUtilities::CeilToInt(MinVoxelVertex), FIntVector(0), Size - 1);
				const FIntVector End = FVoxelUtilities::Clamp(FVoxelUtilities::FloorToInt(MaxVoxelVertex), FIntVector(0), Size - 1);

				// Do intersection counts
				for (int32 Z = Start.Z; Z <= End.Z; Z++)
				{
					for (int32 Y = Start.Y; Y <= End.Y; Y++)
					{
						const auto Get2D = [](const FVector& V) { return FVector2D_Double(V.Y, V.Z); };
						double AlphaA, AlphaB, AlphaC;
						if (PointInTriangle2D(FVector2D_Double(Y, Z), Get2D(VoxelVertexA), Get2D(VoxelVertexB), Get2D(VoxelVertexC), AlphaA, AlphaB, AlphaC))
						{
							const float X = AlphaA * VoxelVertexA.X + AlphaB * VoxelVertexB.X + AlphaC * VoxelVertexC.X; // Intersection X coordinate
#if 1
							IntersectionCount(FMath::Clamp(FMath::CeilToInt(X), 0, Size.X - 1), Y, Z)++;
#else
							const int32 IntervalX = FMath::CeilToInt(X); // Intersection is in (IntervalX - 1, IntervalX]
							if (IntervalX < 0)
							{
								IntersectionCount(FMath::Clamp(FMath::CeilToInt(X), 0, Size.X - 1), Y, Z)++; // We enlarge the first interval to include everything to the -X direction
							}
							else if (IntervalX < Size.X)
							{
								IntersectionCount(IntervalX, Y, Z)++; // We ignore intersections that are beyond the +X side of the grid
							}
#endif
						}
					}
				}
			}
		}
	}

	if (Settings.bDoSweep)
	{
		VOXEL_SCOPE_COUNTER("Sweep");

		// And now we fill in the rest of the distances with fast sweeping
		for (uint32 Pass = 0; Pass < 2; Pass++)
		{
			Sweep(Settings.Triangles, Settings.Vertices, OutPhi, ClosestTriangleIndices, Settings.Origin, Settings.Delta, { +1, +1, +1 });
			Sweep(Settings.Triangles, Settings.Vertices, OutPhi, ClosestTriangleIndices, Settings.Origin, Settings.Delta, { -1, -1, -1 });
			Sweep(Settings.Triangles, Settings.Vertices, OutPhi, ClosestTriangleIndices, Settings.Origin, Settings.Delta, { +1, +1, -1 });
			Sweep(Settings.Triangles, Settings.Vertices, OutPhi, ClosestTriangleIndices, Settings.Origin, Settings.Delta, { -1, -1, +1 });
			Sweep(Settings.Triangles, Settings.Vertices, OutPhi, ClosestTriangleIndices, Settings.Origin, Settings.Delta, { +1, -1, +1 });
			Sweep(Settings.Triangles, Settings.Vertices, OutPhi, ClosestTriangleIndices, Settings.Origin, Settings.Delta, { -1, +1, -1 });
			Sweep(Settings.Triangles, Settings.Vertices, OutPhi, ClosestTriangleIndices, Settings.Origin, Settings.Delta, { +1, -1, -1 });
			Sweep(Settings.Triangles, Settings.Vertices, OutPhi, ClosestTriangleIndices, Settings.Origin, Settings.Delta, { -1, +1, +1 });
		}
	}

	if (Settings.bComputeSign)
	{
		VOXEL_SCOPE_COUNTER("Compute Signs");

		// Then figure out signs (inside/outside) from intersection counts
		OutNumLeaks = 0;
		for (int32 Z = 0; Z < Size.Z; Z++)
		{
			for (int32 Y = 0; Y < Size.Y; Y++)
			{
				if (Settings.bHideLeaks)
				{
					int32 Count = 0;
					for (int32 X = 0; X < Size.X; X++)
					{
						Count += IntersectionCount(X, Y, Z);
					}
					if (Count % 2 == 1)
					{
						OutNumLeaks++;
						// Possible leak, skip instead of creating a long tube
						// Holes are better than having long filled tubes
						continue;
					}
				}
				int32 Count = 0;
				FVector2D LastUV = FVector2D(ForceInit);
				TArray<FColor, TInlineAllocator<2>> LastColors;
				LastColors.SetNumUninitialized(Settings.ColorTextures.Num());
				for (int32 Index = 0; Index < Settings.ColorTextures.Num(); Index++)
				{
					LastColors[Index] = FColor(ForceInit);
				}
				for (int32 X = 0; X < Size.X; ++X)
				{
					Count += IntersectionCount(X, Y, Z);
					if (Count % 2 == 1)
					{
						// If parity of intersections so far is odd, we are inside the mesh
						OutPhi(X, Y, Z) *= -1;
					}
					const bool bComputed = Computed(X, Y, Z);
					if (Settings.bExportUVs)
					{
						if (!bComputed)
						{
							OutUVs(X, Y, Z) = LastUV;
						}
						else
						{
							LastUV = OutUVs(X, Y, Z);
						}
					}
					for (int32 Index = 0; Index < Settings.ColorTextures.Num(); Index++)
					{
						if (!bComputed)
						{
							OutColors[Index](X, Y, Z) = LastColors[Index];
						}
						else
						{
							LastColors[Index] = OutColors[Index](X, Y, Z);
						}
					}
				}
			}
		}
	}
}