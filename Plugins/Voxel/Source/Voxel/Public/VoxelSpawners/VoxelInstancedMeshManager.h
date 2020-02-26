// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelGlobals.h"
#include "VoxelConfigEnums.h"
#include "Templates/SubclassOf.h"
#include "VoxelSpawners/VoxelSpawnerMatrix.h"
#include "VoxelInstancedMeshSettings.h"
#include "VoxelTickable.h"

struct FVoxelHISMBuiltData;
class AActor;
class AVoxelWorld;
class AVoxelSpawnerActor;
class UStaticMesh;
class UVoxelHierarchicalInstancedStaticMeshComponent;
class IVoxelPool;
class FVoxelData;
class FVoxelProcGenManager;
struct FIntBox;

struct VOXEL_API FVoxelInstancedMeshManagerSettings
{
	const TWeakObjectPtr<AActor> ComponentsOwner;
	const int32 NumberOfInstancesPerHISM;
	const TVoxelSharedRef<FIntVector> WorldOffset;
	const TVoxelSharedRef<IVoxelPool> Pool;
	const TVoxelSharedRef<FVoxelProcGenManager> ProcGenManager;
	const uint32 CollisionChunkSize = 32; // Also change in AVoxelWorld::PostEditChangeProperty
	const uint32 CollisionDistanceInChunks;
	const float VoxelSize;

	FVoxelInstancedMeshManagerSettings(
		const AVoxelWorld* World,
		const TVoxelSharedRef<IVoxelPool>& Pool,
		const TVoxelSharedRef<FVoxelProcGenManager>& ProcGenManager);
};

class VOXEL_API FVoxelInstancedMeshManager : public FVoxelTickable, public TVoxelSharedFromThis<FVoxelInstancedMeshManager>
{
public:
	const FVoxelInstancedMeshManagerSettings Settings;

	static TVoxelSharedRef<FVoxelInstancedMeshManager> Create(const FVoxelInstancedMeshManagerSettings& Settings);
	void Destroy();
	~FVoxelInstancedMeshManager();

	void AddInstances(const FVoxelInstancedMeshSettings& MeshSettings, const TArray<FVoxelSpawnerMatrix>& Transforms, const FIntBox& Bounds);
	AVoxelSpawnerActor* SpawnActor(TSubclassOf<AVoxelSpawnerActor> ActorTemplate, TWeakObjectPtr<UStaticMesh> Mesh, FVoxelSpawnerMatrix Matrix) const;
	void SpawnActors(TSubclassOf<AVoxelSpawnerActor> ActorTemplate, TWeakObjectPtr<UStaticMesh> Mesh, const TArray<FVoxelSpawnerMatrix>& Transforms) const;

	TArray<AVoxelSpawnerActor*> SpawnActorsInArea(const FIntBox& Bounds, const FVoxelData& Data, EVoxelSpawnerActorSpawnType SpawnType) const;
	TMap<FVoxelInstancedMeshSettings, TArray<FVoxelSpawnerMatrix>> RemoveActorsInArea(const FIntBox& Bounds, const FVoxelData& Data, EVoxelSpawnerActorSpawnType SpawnType) const;

	void RecomputeMeshPositions();

protected:
	//~ Begin FVoxelTickable Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickableInEditor() const override { return true; }
	//~ End FVoxelTickable Interface

public:
	void HISMBuildTaskCallback(TWeakObjectPtr<UVoxelHierarchicalInstancedStaticMeshComponent> Component, const TVoxelSharedRef<FVoxelHISMBuiltData>& BuiltData);

private:
	struct FQueuedBuildCallback
	{
		TWeakObjectPtr<UVoxelHierarchicalInstancedStaticMeshComponent> Component;
		TVoxelSharedPtr<FVoxelHISMBuiltData> Data;
	};
	TQueue<FQueuedBuildCallback> HISMBuiltDataQueue;

private:
	explicit FVoxelInstancedMeshManager(const FVoxelInstancedMeshManagerSettings& Settings);

	UVoxelHierarchicalInstancedStaticMeshComponent* CreateHISM(const FVoxelInstancedMeshSettings& MeshSettings) const;
	void SetHISMRelativeLocation(UVoxelHierarchicalInstancedStaticMeshComponent& HISM) const;

private:
	TMap<FVoxelInstancedMeshSettings, TArray<TWeakObjectPtr<UVoxelHierarchicalInstancedStaticMeshComponent>>> MeshMap;
};
