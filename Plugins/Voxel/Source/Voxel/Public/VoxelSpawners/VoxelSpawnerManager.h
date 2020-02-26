// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelGlobals.h"
#include "VoxelSpawnerConfig.h"
#include "Containers/Queue.h"
#include "VoxelTickable.h"

class AVoxelWorld;
class AVoxelWorldInterface;
class FVoxelSpawnerProxy;
class FVoxelSpawnerProxyResult;
class IVoxelRenderer;
class IVoxelLODManager;
class FVoxelInstancedMeshManager;
class FVoxelProcGenManager;
class FVoxelDebugManager;
class FVoxelData;
class IVoxelPool;
class FVoxelConstDataAccelerator;
struct FIntBox;
struct FVoxelSpawnerHit;

struct FVoxelSpawnerSettings
{
	// Used for debug
	const TWeakObjectPtr<const AVoxelWorldInterface> VoxelWorldInterface;

	const TVoxelSharedRef<IVoxelPool> Pool;
	const TVoxelSharedRef<FVoxelDebugManager> DebugManager;
	const TVoxelSharedRef<FVoxelData> Data;
	const TVoxelSharedRef<FVoxelInstancedMeshManager> MeshManager;
	const TVoxelSharedRef<FVoxelProcGenManager> ProcGenManager;
	const TVoxelSharedRef<IVoxelLODManager> LODManager;
	const TVoxelSharedRef<IVoxelRenderer> Renderer;
	const TWeakObjectPtr<UVoxelSpawnerConfig> Config;
	const float VoxelSize;
	const TMap<FName, int32> Seeds;
	const float PriorityDuration;

	FVoxelSpawnerSettings(
		const AVoxelWorld* World,
		const TVoxelSharedRef<IVoxelPool>& Pool,
		const TVoxelSharedRef<FVoxelDebugManager>& DebugManager,
		const TVoxelSharedRef<FVoxelData>& Data,
		const TVoxelSharedRef<IVoxelLODManager>& LODManager,
		const TVoxelSharedRef<IVoxelRenderer>& Renderer,
		const TVoxelSharedRef<FVoxelInstancedMeshManager>& MeshManager,
		const TVoxelSharedRef<FVoxelProcGenManager>& ProcGenManager);
};

struct FVoxelSpawnerThreadSafeConfig
{
	EVoxelSpawnerConfigRayWorldType WorldType = EVoxelSpawnerConfigRayWorldType::Flat;
	TArray<FVoxelSpawnerConfigHeightGroup> HeightGroups;
	TArray<FVoxelSpawnerConfigRayGroup> RayGroups;
	TWeakObjectPtr<UVoxelSpawnerConfig> ConfigObject;
};

class VOXEL_API FVoxelSpawnerManager : public FVoxelTickable, public TVoxelSharedFromThis<FVoxelSpawnerManager>
{
public:
	const FVoxelSpawnerSettings Settings;

	static TVoxelSharedRef<FVoxelSpawnerManager> Create(const FVoxelSpawnerSettings& Settings);
	void Destroy();
	~FVoxelSpawnerManager();

	TVoxelSharedPtr<FVoxelSpawnerProxy> GetSpawner(UVoxelSpawner* Spawner) const;

protected:
	//~ Begin FVoxelTickable Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickableInEditor() const override { return true; }
	//~ End FVoxelTickable Interface

private:
	explicit FVoxelSpawnerManager(const FVoxelSpawnerSettings& Settings);

	void SpawnHeightGroup_GameThread(FIntBox Bounds, int32 HeightGroupIndex);
	void SpawnHeightGroup_AnyThread(const FIntBox& Bounds, int32 HeightGroupIndex);

	void SpawnRayGroup_GameThread(FIntBox Bounds, int32 RayGroupIndex);
	void SpawnRayGroup_AnyThread(const FIntBox& Bounds, int32 RayGroupIndex);

	void UpdateTaskCount() const;

	void ProcessHits(
		const FIntBox& Bounds,
		const TMap<UVoxelSpawner*, TArray<FVoxelSpawnerHit>>& HitsMap,
		const FVoxelConstDataAccelerator& Accelerator);

	void FlushAnyThreadQueue();
	void FlushGameThreadQueue();

	TAtomic<int32> CancelTasksCounter;
	FVoxelSpawnerThreadSafeConfig ThreadSafeConfig;
	TMap<UVoxelSpawner*, TVoxelSharedPtr<FVoxelSpawnerProxy>> SpawnersMap;

	// Not single consumer, so need to use TArray with a lock
	FCriticalSection ApplyAnyThreadQueueSection;
	TArray<TVoxelSharedPtr<FVoxelSpawnerProxyResult>> ApplyAnyThreadQueue;

	TQueue<TVoxelSharedPtr<FVoxelSpawnerProxyResult>, EQueueMode::Mpsc> ApplyGameThreadQueue;

	mutable FThreadSafeCounter TaskCounter;

	template<bool bIsHeightTask>
	friend class TVoxelFoliageBuildTask;
};
