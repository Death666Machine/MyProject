// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "Engine/LatentActionManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "IntBox.h"
#include "VoxelPhysicsPartSpawnerInterface.h"
#include "VoxelPhysics.generated.h"

class FVoxelData;
class AVoxelWorld;

struct FVoxelFloatingPart
{
	FIntVector PartCenter;
	TVoxelSharedPtr<FVoxelData> Data;
	TArray<FVoxelPositionValueMaterial> Voxels;
};

struct FVoxelRemoveFloatingPartsResult
{
	FIntBoxWithValidity BoxToUpdate;
	TArray<FVoxelFloatingPart> Parts;
};

UCLASS()
class VOXEL_API UVoxelPhysicsTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Can be run async
	static void RemoveFloatingParts(
		FVoxelData& Data,
		const FIntBox& Bounds,
		int32 MinParts,
		bool bCreateData,
		bool bCreateVoxels,
		bool bDebug,
		TWeakObjectPtr<AVoxelWorld> DebugWorld,
		FVoxelRemoveFloatingPartsResult& OutResult);

	// Must be run on the game thread
	static void SpawnFloatingPartsAndUpdateWorld(
		IVoxelPhysicsPartSpawner& PartSpawner,
		AVoxelWorld* World,
		FVoxelRemoveFloatingPartsResult&& RemoveFloatingPartsResult);

public:
	/**
	 * Apply voxel physics in a section of the voxel world by removing floating parts
	 * @param World					The voxel world
	 * @param Bounds				The bounds to search in (caution: keep this small!)
	 * @param PartSpawner			The part spawner that will handle the spawning of new parts (construct a new object of class VoxelPhysicsPartSpawner_Smthg)
	 *								If null will just remove the floating parts
	 * @param MinParts				The minimum number of parts (inclusive) to have before removing them. This is useful to avoid considering the whole world as a floating part
	 * @param bHideLatentWarnings	Hide latent warnings
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel", meta = (DefaultToSelf = "World", Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", AdvancedDisplay = "MinParts, bDebug, bHideLatentWarnings"))
		static void ApplyVoxelPhysics(
			UObject* WorldContextObject,
			FLatentActionInfo LatentInfo,
			AVoxelWorld* World,
			FIntBox Bounds,
			TScriptInterface<IVoxelPhysicsPartSpawner> PartSpawner,
			int32 MinParts = 2,
			bool bDebug = false,
			bool bHideLatentWarnings = false);
};