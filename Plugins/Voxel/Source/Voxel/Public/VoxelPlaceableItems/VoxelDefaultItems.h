// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelPlaceableItems/VoxelPlaceableItem.h"
#include "VoxelIntVectorUtilities.h"

class VOXEL_API FVoxelPerlinWorm : public FVoxelPlaceableItem
{
public:
	FORCEINLINE static int32 StaticId() { return EVoxelPlaceableItemId::PerlinWorm; }

	const float Radius;
	const FVector Start;
	const FVector Normal;
	const float Length;

	FVoxelPerlinWorm(const FVector& Start, const FVector& End, float Radius)
		: FVoxelPlaceableItem(
			StaticId(),
			FIntBox(
				FVoxelUtilities::FloorToInt(Start.ComponentMin(End) - Radius),
				FVoxelUtilities::CeilToInt(Start.ComponentMax(End) + Radius)),
			0)
		, Radius(Radius)
		, Start(Start)
		, Normal((End - Start).GetSafeNormal())
		, Length((End - Start).Size())
	{
	}

	inline float GetDistance(const FVector& Position) const
	{
		const float T = FVector::DotProduct(Position - Start, Normal);
		if (-Radius <= T && T <= Length + Radius)
		{
			return FMath::Max(0.f, Radius - FVector::Dist(T * Normal + Start, Position));
		}
		else
		{
			return 0;
		}
	}

	inline FVector GetEnd() const
	{
		return Start + Normal * Length;
	}

	virtual FString GetDescription() const override;
	virtual void Save(FArchive& Ar) const override { unimplemented(); }
	virtual bool ShouldBeSaved() const override { return false; } // Don't save disable perlin worms
};

class VOXEL_API FVoxelDisableEditsBoxItem : public FVoxelPlaceableItem
{
public:
	FORCEINLINE static int32 StaticId() { return EVoxelPlaceableItemId::DisableEditsBox; }

	FVoxelDisableEditsBoxItem(const FIntBox& Bounds)
		: FVoxelPlaceableItem(StaticId(), Bounds, 0)
	{
	}

	virtual FString GetDescription() const override;
	virtual void Save(FArchive& Ar) const override { unimplemented(); }
	virtual bool ShouldBeSaved() const override { return false; } // Don't save disable edits boxes
};
