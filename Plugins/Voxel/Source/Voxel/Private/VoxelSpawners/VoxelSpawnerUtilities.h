// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"

struct FIntBox;
struct FVoxelSpawnerThreadSafeConfig;
struct FVoxelSpawnerHit;
class FVoxelSpawnerRayHandler;
class FVoxelConstDataAccelerator;
class UVoxelSpawner;

namespace FVoxelSpawnerUtilities
{
	void SpawnWithRays(
		const TAtomic<int32>& CancelTasksCounter,
		const FVoxelConstDataAccelerator& Accelerator,
		const FVoxelSpawnerThreadSafeConfig& ThreadSafeConfig,
		int32 RayGroupIndex,
		const FIntBox& Bounds,
		const FVoxelSpawnerRayHandler& RayHandler,
		TMap<UVoxelSpawner*, TArray<FVoxelSpawnerHit>>& OutHits);

	void SpawnWithHeight(
		const TAtomic<int32>& CancelTasksCounter,
		const FVoxelConstDataAccelerator& Accelerator,
		const FVoxelSpawnerThreadSafeConfig& ThreadSafeConfig,
		int32 HeightGroupIndex,
		const FIntBox& Bounds,
		TMap<UVoxelSpawner*, TArray<FVoxelSpawnerHit>>& OutHits);
}
