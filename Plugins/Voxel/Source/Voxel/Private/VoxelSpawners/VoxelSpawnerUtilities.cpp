// Copyright 2020 Phyronnaz

#include "VoxelSpawnerUtilities.h"
#include "IntBox.h"
#include "VoxelData/VoxelData.h"
#include "VoxelData/VoxelDataAccelerator.h"
#include "VoxelSpawners/VoxelSpawnerConfig.h"
#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelSpawners/VoxelSpawner.h"
#include "VoxelSpawners/VoxelSpawnerRandomGenerator.h"
#include "VoxelSpawners/VoxelSpawnerRayHandler.h"
#include "VoxelWorld.h"

inline v_flt GetDefaultDensity(const FVoxelSpawnerConfigElement_Base& Element)
{
	if (Element.DensityGraphOutputName.Name == STATIC_FNAME("Constant 1"))
	{
		return 1;
	}
	else
	{
		// Will also handle Constant 0
		return 0;
	}
}

template<typename T>
inline TUniquePtr<FVoxelSpawnerRandomGenerator> GetRandomGenerator(const T& Element)
{
	if (Element.Advanced.RandomGenerator == EVoxelSpawnerConfigElementRandomGenerator::Sobol)
	{
		return MakeUnique<FVoxelSpawnerSobolRandomGenerator>();
	}
	else
	{
		check(Element.Advanced.RandomGenerator == EVoxelSpawnerConfigElementRandomGenerator::Halton);
		return MakeUnique<FVoxelSpawnerHaltonRandomGenerator>();
	}
}

inline FIntVector GetClosestNotEmptyPoint(const FVoxelConstDataAccelerator& Accelerator, const FVector& Position)
{
	FIntVector ClosestPoint;
	float Distance = MAX_flt;
	for (auto& Neighbor : FVoxelUtilities::GetNeighbors(Position))
	{
		if (!Accelerator.GetValue(Neighbor, 0).IsEmpty())
		{
			const float PointDistance = (FVector(Neighbor) - Position).SizeSquared();
			if (PointDistance < Distance)
			{
				Distance = PointDistance;
				ClosestPoint = Neighbor;
			}
		}
	}
	if (/*ensure*/(Distance < 100))
	{
		return ClosestPoint;
	}
	else
	{
		return FVoxelUtilities::RoundToInt(Position);
	}
}
inline void GetSphereBasisFromBounds(
	const FIntBox& Bounds,
	FVector& OutBasisX,
	FVector& OutBasisY,
	FVector& OutBasisZ)
{
	// Find closest corner
	const FVector Direction = -Bounds.GetCenter().GetSafeNormal();

	const FVector AbsDirection = Direction.GetAbs();
	const float Max = AbsDirection.GetMax();
	const FVector Vector =
		Max == AbsDirection.X
		? FVector(0, 1, 0)
		: Max == AbsDirection.Y
		? FVector(0, 0, 1)
		: FVector(1, 0, 0);

	OutBasisX = Direction ^ Vector;
	OutBasisY = Direction ^ OutBasisX;
	OutBasisZ = Direction;

	OutBasisX.Normalize();
	OutBasisY.Normalize();

	ensure(OutBasisX.GetAbsMax() > KINDA_SMALL_NUMBER);
	ensure(OutBasisY.GetAbsMax() > KINDA_SMALL_NUMBER);
}

inline void GetBasisFromBounds(
	const FVoxelSpawnerThreadSafeConfig& ThreadSafeConfig,
	const FIntBox& Bounds,
	FVector& OutBasisX,
	FVector& OutBasisY)
{
	if (ThreadSafeConfig.WorldType == EVoxelSpawnerConfigRayWorldType::Flat)
	{
		OutBasisX = FVector::RightVector;
		OutBasisY = FVector::ForwardVector;
	}
	else
	{
		check(ThreadSafeConfig.WorldType == EVoxelSpawnerConfigRayWorldType::Sphere);
		FVector OutBasisZ;
		GetSphereBasisFromBounds(Bounds, OutBasisX, OutBasisY, OutBasisZ);

		OutBasisX *= 1.5; // Hack to avoid holes
		OutBasisY *= 1.5;
	}
}

inline FVector GetRayDirection(
	const FVoxelSpawnerThreadSafeConfig& ThreadSafeConfig,
	const FVector& Start,
	const FIntVector& ChunkPosition)
{
	if (ThreadSafeConfig.WorldType == EVoxelSpawnerConfigRayWorldType::Flat)
	{
		return -FVector::UpVector;
	}
	else
	{
		check(ThreadSafeConfig.WorldType == EVoxelSpawnerConfigRayWorldType::Sphere);
		return -(FVector(ChunkPosition) + Start).GetSafeNormal();
	}
}

void FVoxelSpawnerUtilities::SpawnWithRays(
	const TAtomic<int32>& CancelTasksCounter,
	const FVoxelConstDataAccelerator& Accelerator,
	const FVoxelSpawnerThreadSafeConfig& ThreadSafeConfig,
	int32 RayGroupIndex,
	const FIntBox& Bounds,
	const FVoxelSpawnerRayHandler& RayHandler,
	TMap<UVoxelSpawner*, TArray<FVoxelSpawnerHit>>& OutHits)
{
	VOXEL_FUNCTION_COUNTER();

	auto& RayGroup = ThreadSafeConfig.RayGroups[RayGroupIndex];

	check(Bounds.Size().X == Bounds.Size().Y && Bounds.Size().Y == Bounds.Size().Z);
	const int32 BoundsSize = Bounds.Size().X;
	const FIntVector& ChunkPosition = Bounds.Min;
	const uint32 Seed = Bounds.GetMurmurHash();

	FVector BasisX, BasisY;
	GetBasisFromBounds(ThreadSafeConfig, Bounds, BasisX, BasisY);

	for (int32 ElementIndex = 0; ElementIndex < RayGroup.Spawners.Num(); ElementIndex++)
	{
		auto& Element = RayGroup.Spawners[ElementIndex];

		const uint32 ElementSeed = FVoxelUtilities::MurmurHash32(Seed, RayGroupIndex, ElementIndex, Element.FinalSeed, /* avoid collisions with height spawners */23);
		const FRandomStream RandomStream(ElementSeed);

		const TUniquePtr<FVoxelSpawnerRandomGenerator> RandomGenerator = GetRandomGenerator(Element);
		RandomGenerator->Init(
			ElementSeed ^ FVoxelUtilities::MurmurHash32(ChunkPosition.X),
			ElementSeed ^ FVoxelUtilities::MurmurHash32(ChunkPosition.Y));

		const int32 NumRays = FMath::FloorToInt(FMath::Square(double(BoundsSize) / double(Element.DistanceBetweenInstancesInVoxel)));

		struct FVoxelRay
		{
			FVector Position;
			FVector Direction;
		};
		TArray<FVoxelSpawnerHit> Hits;
		TArray<FVoxelRay> QueuedRays;
		for (int32 Index = 0; Index < NumRays; Index++)
		{
			if ((Index & 0xFF) == 0 && CancelTasksCounter.Load(EMemoryOrder::Relaxed) != 0)
			{
				return;
			}

			const FVector2D RandomValue = 2 * RandomGenerator->GetValue() - 1; // Map from 0,1 to -1,1
			const FVector Start = BoundsSize / 2 * (BasisX * RandomValue.X + BasisY * RandomValue.Y + 1); // +1: we want to be in the center
			const FVector Direction = GetRayDirection(ThreadSafeConfig, Start, ChunkPosition);
			FVoxelSpawnerHit Hit;
			if (RayHandler.TraceRay(
				Start - Direction * 4 * BoundsSize /* Ray offset */,
				Direction,
				Hit.Normal,
				Hit.Position))
			{
				Hits.Add(Hit);
				QueuedRays.Add({ Hit.Position, Direction });
			}
			RandomGenerator->Next();
		}

		// Process consecutive hits
		while (QueuedRays.Num() > 0)
		{
			const auto Ray = QueuedRays.Pop(false);
			const float Offset = 1;
			FVoxelSpawnerHit Hit;
			if (RayHandler.TraceRay(
				Ray.Position + Ray.Direction * Offset,
				Ray.Direction,
				Hit.Normal,
				Hit.Position))
			{
				Hits.Add(Hit);
				QueuedRays.Add({ Hit.Position, Ray.Direction });
			}
		}

		for (auto& Hit : Hits)
		{
			const FVector& LocalPosition = Hit.Position;
			const FVector& Normal = Hit.Normal;
			const FVector GlobalPosition = FVector(ChunkPosition) + LocalPosition;

			if (!Accelerator.Data.IsInWorld(GlobalPosition))
			{
				continue;
			}

			const FIntVector VoxelPosition = GetClosestNotEmptyPoint(Accelerator, GlobalPosition);

			const auto GetGeneratorDensity = [&]()
			{
				// Need to get the right ItemHolder
				return Accelerator.GetCustomOutput<v_flt>(GetDefaultDensity(Element), Element.DensityGraphOutputName, VoxelPosition.X, VoxelPosition.Y, VoxelPosition.Z, 0);
			};

			float Density;
			if (Element.Advanced.Channel == EVoxelSpawnerChannel::None)
			{
				Density = GetGeneratorDensity();
			}
			else
			{
				const auto Layer = GetRGBALayerFromSpawnerChannel(Element.Advanced.Channel);
				const FVoxelFoliage Foliage = Accelerator.Get<FVoxelFoliage>(VoxelPosition, 0);
				if (Foliage.IsChannelSet(Layer))
				{
					Density = Foliage.GetChannelValue(Layer);
				}
				else
				{
					Density = GetGeneratorDensity();
				}
			}

			if (RandomStream.GetFraction() <= Density)
			{
				OutHits.FindOrAdd(Element.Spawner).Emplace(FVoxelSpawnerHit(GlobalPosition, Normal));
			}
		}
	}
}

void FVoxelSpawnerUtilities::SpawnWithHeight(
	const TAtomic<int32>& CancelTasksCounter,
	const FVoxelConstDataAccelerator& Accelerator,
	const FVoxelSpawnerThreadSafeConfig& ThreadSafeConfig,
	const int32 HeightGroupIndex,
	const FIntBox& Bounds,
	TMap<UVoxelSpawner*, TArray<FVoxelSpawnerHit>>& OutHits)
{
	VOXEL_FUNCTION_COUNTER();

	// Note: assets are ignored when querying the height and the density, as it gets way too messy
	// For flat worlds, the height and the density is queried at Z = 0
	// For sphere worlds at a normalized (X, Y, Z)
	// In theory the density could be computed at the exact position if bComputeDensityFirst is false, but this makes the behavior unpredictable

	auto& HeightGroup = ThreadSafeConfig.HeightGroups[HeightGroupIndex];

	check(Bounds.Size().GetMin() == HeightGroup.ChunkSize);
	check(Bounds.Size().GetMax() == HeightGroup.ChunkSize);

	const FIntBox BoundsLimit = Bounds.Overlap(Accelerator.Data.WorldBounds);

	const FIntVector& ChunkPosition = Bounds.Min;
	const uint32 Seed = Bounds.GetMurmurHash();

	constexpr v_flt DefaultHeight = 0;

	auto& WorldGenerator = *Accelerator.Data.WorldGenerator;
	const auto Range = WorldGenerator.GetCustomOutputRange<v_flt>(
		DefaultHeight, // This is the value to use if the generator doesn't have the custom output.
		HeightGroup.HeightGraphOutputName,
		Bounds,
		0,
		FVoxelItemStack::Empty);

	const bool bIsSphere = ThreadSafeConfig.WorldType == EVoxelSpawnerConfigRayWorldType::Sphere;

	if (bIsSphere)
	{
		auto Corners = Bounds.GetCorners(0);
		if (!Range.Intersects(TVoxelRange<v_flt>::FromList(
			FVector(Corners[0]).Size(),
			FVector(Corners[1]).Size(),
			FVector(Corners[2]).Size(),
			FVector(Corners[3]).Size(),
			FVector(Corners[4]).Size(),
			FVector(Corners[5]).Size(),
			FVector(Corners[6]).Size(),
			FVector(Corners[7]).Size())))
		{
			return;
		}
	}
	else
	{
		if (!Range.Intersects(TVoxelRange<v_flt>(Bounds.Min.Z, Bounds.Max.Z)))
		{
			return;
		}
	}

	for (int32 ElementIndex = 0; ElementIndex < HeightGroup.Spawners.Num(); ElementIndex++)
	{
		auto& Element = HeightGroup.Spawners[ElementIndex];

		const uint32 ElementSeed = FVoxelUtilities::MurmurHash32(Seed, HeightGroupIndex, ElementIndex, Element.FinalSeed);
		const FRandomStream RandomStream(ElementSeed);

		const TUniquePtr<FVoxelSpawnerRandomGenerator> RandomGenerator = GetRandomGenerator(Element);
		RandomGenerator->Init(
			ElementSeed ^ FVoxelUtilities::MurmurHash32(ChunkPosition.X),
			ElementSeed ^ FVoxelUtilities::MurmurHash32(ChunkPosition.Y));

		const int32 NumRays = FMath::FloorToInt(FMath::Square(double(HeightGroup.ChunkSize) / double(Element.DistanceBetweenInstancesInVoxel)));

		if (bIsSphere)
		{
			const FVector Center = Bounds.GetCenter();
			for (int32 Index = 0; Index < NumRays; Index++)
			{
				if ((Index & 0xFF) == 0 && CancelTasksCounter.Load(EMemoryOrder::Relaxed) != 0)
				{
					return;
				}

				FVector BasisX, BasisY, BasisZ;
				GetSphereBasisFromBounds(Bounds, BasisX, BasisY, BasisZ);

				const FVector2D RandomValue = 2 * RandomGenerator->GetValue() - 1; // Map from 0,1 to -1,1
				const auto SamplePosition = [&](float X, float Y)
				{
					// 1.5f: hack to avoid holes between chunks
					return (Center + 1.5f * HeightGroup.ChunkSize / 2.f * (BasisX * X + BasisY * Y)).GetSafeNormal();
				};
				const FVector Start = SamplePosition(RandomValue.X, RandomValue.Y);

				const auto Lambda = [&](const FVector& Position)
				{
					const FVector Left = SamplePosition(RandomValue.X - 1.f / HeightGroup.ChunkSize, RandomValue.Y);
					const FVector Right = SamplePosition(RandomValue.X + 1.f / HeightGroup.ChunkSize, RandomValue.Y);
					const FVector Bottom = SamplePosition(RandomValue.X, RandomValue.Y - 1.f / HeightGroup.ChunkSize);
					const FVector Top = SamplePosition(RandomValue.X, RandomValue.Y + 1.f / HeightGroup.ChunkSize);
					const FVector Gradient =
						BasisX * (
							WorldGenerator.GetCustomOutput<v_flt>(DefaultHeight, HeightGroup.HeightGraphOutputName, Left, 0, FVoxelItemStack::Empty) -
							WorldGenerator.GetCustomOutput<v_flt>(DefaultHeight, HeightGroup.HeightGraphOutputName, Right, 0, FVoxelItemStack::Empty)) +
						BasisY * (
							WorldGenerator.GetCustomOutput<v_flt>(DefaultHeight, HeightGroup.HeightGraphOutputName, Bottom, 0, FVoxelItemStack::Empty) -
							WorldGenerator.GetCustomOutput<v_flt>(DefaultHeight, HeightGroup.HeightGraphOutputName, Top, 0, FVoxelItemStack::Empty)) +
						BasisZ * -2.f;
					OutHits.FindOrAdd(Element.Spawner).Emplace(FVoxelSpawnerHit(Position, Gradient.GetSafeNormal()));
				};

				if (Element.Advanced.bComputeDensityFirst)
				{
					const v_flt Density = WorldGenerator.GetCustomOutput<v_flt>(GetDefaultDensity(Element), Element.DensityGraphOutputName, Start, 0, FVoxelItemStack::Empty);
					if (RandomStream.GetFraction() <= Density)
					{
						const v_flt Height = WorldGenerator.GetCustomOutput<v_flt>(DefaultHeight, HeightGroup.HeightGraphOutputName, Start, 0, FVoxelItemStack::Empty);
						const FVector Position = Start * Height;
						if (BoundsLimit.ContainsFloat(Position))
						{
							Lambda(Position);
						}
					}
				}
				else
				{
					const v_flt Height = WorldGenerator.GetCustomOutput<v_flt>(DefaultHeight, HeightGroup.HeightGraphOutputName, Start, 0, FVoxelItemStack::Empty);
					const FVector Position = Start * Height;
					if (BoundsLimit.ContainsFloat(Position))
					{
						const v_flt Density = WorldGenerator.GetCustomOutput<v_flt>(GetDefaultDensity(Element), Element.DensityGraphOutputName, Start, 0, FVoxelItemStack::Empty);
						if (RandomStream.GetFraction() <= Density)
						{
							Lambda(Position);
						}
					}
				}
				RandomGenerator->Next();
			}
		}
		else
		{
			for (int32 Index = 0; Index < NumRays; Index++)
			{
				if ((Index & 0xFF) == 0 && CancelTasksCounter.Load(EMemoryOrder::Relaxed) != 0)
				{
					return;
				}

				const FVector2D LocalPosition = RandomGenerator->GetValue() * HeightGroup.ChunkSize;
				const v_flt X = v_flt(LocalPosition.X) + Bounds.Min.X;
				const v_flt Y = v_flt(LocalPosition.Y) + Bounds.Min.Y;
				const auto Lambda = [&](v_flt Z)
				{
					FVector Gradient;
					Gradient.X =
						WorldGenerator.GetCustomOutput<v_flt>(DefaultHeight, HeightGroup.HeightGraphOutputName, X - 1, Y, 0, 0, FVoxelItemStack::Empty) -
						WorldGenerator.GetCustomOutput<v_flt>(DefaultHeight, HeightGroup.HeightGraphOutputName, X + 1, Y, 0, 0, FVoxelItemStack::Empty);
					Gradient.Y =
						WorldGenerator.GetCustomOutput<v_flt>(DefaultHeight, HeightGroup.HeightGraphOutputName, X, Y - 1, 0, 0, FVoxelItemStack::Empty) -
						WorldGenerator.GetCustomOutput<v_flt>(DefaultHeight, HeightGroup.HeightGraphOutputName, X, Y + 1, 0, 0, FVoxelItemStack::Empty);
					Gradient.Z = 2;
					OutHits.FindOrAdd(Element.Spawner).Emplace(FVoxelSpawnerHit(FVector(X, Y, Z), Gradient.GetSafeNormal()));
				};
				if (Element.Advanced.bComputeDensityFirst)
				{
					const v_flt Density = WorldGenerator.GetCustomOutput<v_flt>(GetDefaultDensity(Element), Element.DensityGraphOutputName, X, Y, 0, 0, FVoxelItemStack::Empty);
					if (RandomStream.GetFraction() <= Density)
					{
						const v_flt Z = WorldGenerator.GetCustomOutput<v_flt>(DefaultHeight, HeightGroup.HeightGraphOutputName, X, Y, 0, 0, FVoxelItemStack::Empty);
						if (BoundsLimit.Min.Z <= Z && Z <= BoundsLimit.Max.Z)
						{
							Lambda(Z);
						}
					}
				}
				else
				{
					const v_flt Z = WorldGenerator.GetCustomOutput<v_flt>(DefaultHeight, HeightGroup.HeightGraphOutputName, X, Y, 0, 0, FVoxelItemStack::Empty);
					if (BoundsLimit.Min.Z <= Z && Z <= BoundsLimit.Max.Z)
					{
						const v_flt Density = WorldGenerator.GetCustomOutput<v_flt>(GetDefaultDensity(Element), Element.DensityGraphOutputName, X, Y, 0, 0, FVoxelItemStack::Empty);
						if (RandomStream.GetFraction() <= Density)
						{
							Lambda(Z);
						}
					}
				}
				RandomGenerator->Next();
			}
		}
	}
}