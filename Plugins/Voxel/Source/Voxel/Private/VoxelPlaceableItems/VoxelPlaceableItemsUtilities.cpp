// Copyright 2020 Phyronnaz

#include "VoxelPlaceableItems/VoxelPlaceableItemsUtilities.h"
#include "VoxelPlaceableItems/VoxelDefaultItems.h"
#include "VoxelTools/VoxelToolHelpers.h"
#include "VoxelData/VoxelData.h"
#include "VoxelWorld.h"
#include "FastNoise.h"
#include "VoxelMessages.h"

void UVoxelPlaceableItemsUtilities::AddWorms(
	AVoxelWorld* World,
	float Radius,
	int32 Seed,
	FVector RotationAmplitude,
	FVector NoiseDir,
	float NoiseSegmentLength,
	FVector Start,
	FVector InitialDir,
	float VoxelSegmentLength,
	int32 NumSegments,
	float SplitProbability,
	float SplitProbabilityGain,
	int32 BranchMeanSize,
	int32 BranchSizeVariation)
{
	VOXEL_PRO_ONLY_VOID();
	CHECK_VOXELWORLD_IS_CREATED_VOID();

	auto& Data = World->GetData();

	static bool bIsFirst = true;
	static FRandomStream Stream;

	bool bIsFirstLocal = bIsFirst;

	if (bIsFirstLocal)
	{
		Stream = FRandomStream(Seed);
		bIsFirst = false;
	}

	FVoxelWriteScopeLock Lock(Data, FIntBox::Infinite, STATIC_FNAME("AddWorms"), bIsFirstLocal);

	FastNoise ModuleX;
	FastNoise ModuleY;
	FastNoise ModuleZ;

	ModuleX.SetSeed(Seed);
	ModuleY.SetSeed(Seed + 1);
	ModuleZ.SetSeed(Seed + 2);

	FVector CurrentPosition = Start;
	FVector CurrentDir = InitialDir;
	for (int32 I = 0; I < NumSegments; I++)
	{
		FVector NewPosition = CurrentPosition + CurrentDir * VoxelSegmentLength;
		Data.AddItem<FVoxelPerlinWorm>(CurrentPosition, NewPosition, Radius);
		CurrentPosition = NewPosition;

		FVector NoisePosition = NoiseDir * NoiseSegmentLength * I;
		CurrentDir = CurrentDir.RotateAngleAxis(RotationAmplitude.X * ModuleX.GetSimplex_3D(NoisePosition.X, NoisePosition.Y, NoisePosition.Z, 0.02), FVector(1, 0, 0));
		CurrentDir = CurrentDir.RotateAngleAxis(RotationAmplitude.Y * ModuleY.GetSimplex_3D(NoisePosition.X, NoisePosition.Y, NoisePosition.Z, 0.02), FVector(0, 1, 0));
		CurrentDir = CurrentDir.RotateAngleAxis(RotationAmplitude.Z * ModuleZ.GetSimplex_3D(NoisePosition.X, NoisePosition.Y, NoisePosition.Z, 0.02), FVector(0, 0, 1));

		if (Stream.FRand() < SplitProbability)
		{
			AddWorms(
				World,
				Radius,
				Seed + 10,
				RotationAmplitude,
				NoiseDir,
				NoiseSegmentLength,
				CurrentPosition,
				CurrentDir.RotateAngleAxis((Stream.FRand() * 2 - 1) * RotationAmplitude.X, FVector(1, 0, 0)).RotateAngleAxis((Stream.FRand() * 2 - 1) * RotationAmplitude.Y, FVector(0, 1, 0)).RotateAngleAxis((Stream.FRand() * 2 - 1) * RotationAmplitude.Z, FVector(0, 0, 1)),
				VoxelSegmentLength,
				FMath::Min<int32>(BranchMeanSize + (Stream.FRand() * 2 - 1) * BranchSizeVariation, NumSegments - (I + 1)),
				SplitProbability * SplitProbabilityGain,
				SplitProbabilityGain);
		}
	}

	if (bIsFirstLocal)
	{
		bIsFirst = true;
	}
}