// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "IntBox.h"
#include "VoxelGlobals.h"
#include "VoxelConfigEnums.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "VoxelSpawners/VoxelSpawnerMatrix.h"
#include "VoxelHierarchicalInstancedStaticMeshComponent.generated.h"

struct FVoxelHISMBuiltData;
class IVoxelPool;
class FVoxelConstDataAccelerator;
class FVoxelInstancedMeshManager;

// Need to prefix names with Voxel to avoid collisions with normal HISM
UCLASS()
class VOXEL_API UVoxelHierarchicalInstancedStaticMeshComponent : public UHierarchicalInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	// How long to wait for new instances before triggering a new cull tree/render update
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
		float Voxel_BuildDelay = 0.5f;

public:
	UVoxelHierarchicalInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer);

	~UVoxelHierarchicalInstancedStaticMeshComponent();

public:
	void Init(
		TVoxelWeakPtr<IVoxelPool> Pool,
		TVoxelWeakPtr<FVoxelInstancedMeshManager> InstancedMeshManager,
		float VoxelSize);

private:
	TVoxelWeakPtr<IVoxelPool> Voxel_Pool;
	TVoxelWeakPtr<FVoxelInstancedMeshManager> Voxel_InstancedMeshManager;
	float Voxel_VoxelSize = 0;

	friend class FVoxelHISMBuildTask;

public:
	void Voxel_AppendTransforms(const TArray<FVoxelSpawnerMatrix>& InTransforms, const FIntBox& Bounds);
	void Voxel_StartBuildTree();
	void Voxel_FinishBuilding(FVoxelHISMBuiltData& BuiltData);

	void Voxel_EnablePhysics(FIntBox Chunk);
	void Voxel_DisablePhysics(FIntBox Chunk);
	void Voxel_RefreshPhysics(const FIntBox& BoundsToUpdate);

	void Voxel_RemoveMeshesInArea(
		const FIntBox& VoxelBounds,
		const FVoxelConstDataAccelerator* Accelerator,
		EVoxelSpawnerActorSpawnType SpawnType,
		TArray<FVoxelSpawnerMatrix>& OutMatrices);

	int32 Voxel_GetNumInstances() const
	{
		return Voxel_UnbuiltMatrices.Num();
	}

public:
	template<typename T1, typename T2>
	inline void Voxel_IterateInstancesInBounds(const TArray<FClusterNode>& ClusterTree, const T1& Bounds, T2 Lambda) const
	{
		VOXEL_FUNCTION_COUNTER();
		Voxel_IterateInstancesInBoundsImpl(ClusterTree, Bounds, Lambda, 0);
	}
	template<typename T1, typename T2>
	void Voxel_IterateInstancesInBoundsImpl(const TArray<FClusterNode>& ClusterTree, const T1& Bounds, T2 Lambda, const int32 Index) const
	{
		struct FHelper
		{
			inline static bool ContainsBounds(const FIntBox& A, const FBox& B) { return A.ContainsTemplate(B); }
			inline static bool ContainsBounds(const FBox& A, const FBox& B) { return A.IsInside(B); }
		};

		if (ClusterTree.Num() == 0)
		{
			return;
		}
		auto& Tree = ClusterTree[Index];
		const auto TreeBounds = FBox(Tree.BoundMin, Tree.BoundMax);
		if (Bounds.Intersect(TreeBounds))
		{
			if (FHelper::ContainsBounds(Bounds, TreeBounds) || Tree.FirstChild < 0)
			{
				for (int32 Instance = Tree.FirstInstance; Instance <= Tree.LastInstance; Instance++)
				{
					Lambda(Instance);
				}
			}
			else
			{
				for (int32 Child = Tree.FirstChild; Child <= Tree.LastChild; Child++)
				{
					Voxel_IterateInstancesInBoundsImpl(ClusterTree, Bounds, Lambda, Child);
				}
			}
		}
	}

public:
	//~ Begin UActorComponent Interface
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void OnDestroyPhysicsState() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

private:
	TArray<FVoxelSpawnerMatrix> Voxel_UnbuiltMatrices;
	TArray<FVoxelSpawnerMatrix> Voxel_BuiltMatrices;
	TArray<int32> Voxel_InstancesToBuiltInstances;
	TArray<int32> Voxel_BuiltInstancesToInstances;

	// Unbuilt instances indices to clear when task finishes
	TArray<int32> Voxel_UnbuiltInstancesToClear;

	uint64 Voxel_TaskUniqueId = 0;
	TVoxelSharedPtr<FThreadSafeCounter> Voxel_TaskCancelCounterPtr;

	FTimerHandle Voxel_TimerHandle;

	// Instance bounds to rebuild physics on when the respective tasks will be done
	TMap<uint64, TArray<FIntBox>> Voxel_TaskIdToNewInstancesBounds;
	// Instances bounds to rebuild physics on when the next task will be done
	TArray<FIntBox> Voxel_PendingNewInstancesBounds;

	TMap<FIntBox, TArray<FBodyInstance*>> Voxel_InstanceBodies;

	uint32 Voxel_AllocatedMemory = 0;

	void UpdateAllocatedMemory();

	// These 2 do not modify Voxel_InstanceBodies. Used by RefreshPhysics.
	void Voxel_EnablePhysicsImpl(const FIntBox& Chunk, TArray<FBodyInstance*>& OutBodies) const;
	void Voxel_DisablePhysicsImpl(TArray<FBodyInstance*>& Bodies) const;
};
