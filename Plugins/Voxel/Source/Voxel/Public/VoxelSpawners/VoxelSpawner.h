// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "IntBox.h"
#include "VoxelGlobals.h"
#include "VoxelSpawner.generated.h"

class FVoxelConstDataAccelerator;
class FVoxelSpawnerManager;
class FVoxelSpawnerProxy;
class FVoxelData;
class AVoxelSpawnerActor;
class UVoxelSpawner;

struct FVoxelSpawnerHit
{
	FVector Position;
	FVector Normal;

	FVoxelSpawnerHit() = default;
	FVoxelSpawnerHit(const FVector& Position, const FVector& Normal)
		: Position(Position)
		, Normal(Normal)
	{
	}
};

class VOXEL_API FVoxelSpawnerProxyResult : public TVoxelSharedFromThis<FVoxelSpawnerProxyResult>
{
public:
	FVoxelSpawnerManager& Manager;

	explicit FVoxelSpawnerProxyResult(const FVoxelSpawnerProxy& Proxy);
	virtual ~FVoxelSpawnerProxyResult() = default;

	FVoxelSpawnerProxyResult(const FVoxelSpawnerProxyResult&) = delete;
	FVoxelSpawnerProxyResult& operator=(const FVoxelSpawnerProxyResult&) = delete;

	//~ Begin FVoxelSpawnerProxyResult Interface
	// virtual void Destroy() = 0;
	// virtual void Serialize() = 0;

	// Called once the data has been unlocked, use this if you need to lock the data
	virtual void Apply_AnyThread() {}
	virtual void Apply_GameThread() {}
	//~ End FVoxelSpawnerProxyResult Interface
};

class VOXEL_API FVoxelSpawnerProxy : public TVoxelSharedFromThis<FVoxelSpawnerProxy>
{
public:
	FVoxelSpawnerManager& Manager;
	const uint32 SpawnerSeed;

	FVoxelSpawnerProxy(UVoxelSpawner* Spawner, FVoxelSpawnerManager& Manager, uint32 Seed = 0);
	virtual ~FVoxelSpawnerProxy() = default;

	// Both of these functions must be called only from the Manager or recursively!

	//~ Begin FVoxelSpawnerProxy Interface
	virtual TUniquePtr<FVoxelSpawnerProxyResult> ProcessHits(
		const FIntBox& Bounds,
		const TArray<FVoxelSpawnerHit>& Hits,
		const FVoxelConstDataAccelerator& Accelerator) const = 0;
	virtual void PostSpawn() = 0; // Called right after every spawner is created
	//~ End FVoxelSpawnerProxy Interface
};

UCLASS(Abstract)
class VOXEL_API UVoxelSpawner : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Placement", meta = (ClampMin = 0))
		float DistanceBetweenInstancesInVoxel = 10;

public:
	virtual TVoxelSharedRef<FVoxelSpawnerProxy> GetSpawnerProxy(FVoxelSpawnerManager& Manager);
	// All added spawners MUST be valid. Returns success
	virtual bool GetSpawners(TSet<UVoxelSpawner*>& OutSpawners);
	virtual float GetDistanceBetweenInstancesInVoxel() const { return DistanceBetweenInstancesInVoxel; }
};