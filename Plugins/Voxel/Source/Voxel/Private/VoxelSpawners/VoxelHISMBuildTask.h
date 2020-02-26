// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelGlobals.h"
#include "VoxelAsyncWork.h"
#include "VoxelSpawners/VoxelSpawnerMatrix.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

class FVoxelInstancedMeshManager;
class UVoxelHierarchicalInstancedStaticMeshComponent;

struct FVoxelHISMBuiltData
{
	uint64 UniqueId;

	TArray<FVoxelSpawnerMatrix> BuiltInstancesMatrices;

	TArray<int32> InstancesToBuiltInstances;
	TArray<int32> BuiltInstancesToInstances;

	TUniquePtr<FStaticMeshInstanceData> InstanceBuffer;
	TArray<FClusterNode> ClusterTree;
	int32 OcclusionLayerNum = 0;
};

// Will auto delete
class FVoxelHISMBuildTask : public FVoxelAsyncWork
{
public:
	const uint64 UniqueId;
	const TVoxelSharedRef<FThreadSafeCounter> CancelCounter;

	const FBox MeshBox;
	const int32 DesiredInstancesPerLeaf;
	const TVoxelWeakPtr<FVoxelInstancedMeshManager> InstancedMeshManager;
	const TWeakObjectPtr<UVoxelHierarchicalInstancedStaticMeshComponent> Component;

	// Output
	const TVoxelSharedRef<FVoxelHISMBuiltData> BuiltData;

	FVoxelHISMBuildTask(
		UVoxelHierarchicalInstancedStaticMeshComponent* Component,
		const TArray<FVoxelSpawnerMatrix>& Matrices);

	//~ Begin FVoxelAsyncWork Interface
	virtual void DoWork() override;
	virtual uint32 GetPriority() const override;
	//~ End FVoxelAsyncWork Interface
};
