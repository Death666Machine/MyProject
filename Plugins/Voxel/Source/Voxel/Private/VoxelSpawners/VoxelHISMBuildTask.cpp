// Copyright 2020 Phyronnaz

#include "VoxelSpawners/VoxelHISMBuildTask.h"
#include "VoxelSpawners/VoxelHierarchicalInstancedStaticMeshComponent.h"
#include "VoxelSpawners/VoxelInstancedMeshManager.h"
#include "VoxelThreadingUtilities.h"

#include "Engine/StaticMesh.h"

static TAutoConsoleVariable<int32> CVarLogHISMBuildTimes(
	TEXT("voxel.spawners.LogCullingTreeBuildTimes"),
	0,
	TEXT("If true, will log all the HISM build times"),
	ECVF_Default);

// NOTE: start of HISM build tree refactor:
// https://github.com/Phyronnaz/VoxelPrivate/blob/fb7cd3edc0ca01dd6b7e7ab9b6908c7c31c1f290/Source/Voxel/Private/VoxelSpawners/VoxelHierarchicalInstancedStaticMeshComponent.cpp#L41

FVoxelHISMBuildTask::FVoxelHISMBuildTask(
	UVoxelHierarchicalInstancedStaticMeshComponent* Component,
	const TArray<FVoxelSpawnerMatrix>& Matrices)
	: FVoxelAsyncWork(STATIC_FNAME("HISM Build Task"), 1e9, true)
	, UniqueId(UNIQUE_ID())
	, CancelCounter(MakeVoxelShared<FThreadSafeCounter>())
	, MeshBox(Component->GetStaticMesh()->GetBounds().GetBox())
	, DesiredInstancesPerLeaf(Component->DesiredInstancesPerLeaf())
	, InstancedMeshManager(Component->Voxel_InstancedMeshManager)
	, Component(Component)
	, BuiltData(MakeVoxelShared<FVoxelHISMBuiltData>())
{
	check(Matrices.Num() > 0);
	BuiltData->UniqueId = UniqueId;
	BuiltData->BuiltInstancesMatrices = Matrices;
}

void FVoxelHISMBuildTask::DoWork()
{
	const double StartTime = FPlatformTime::Seconds();
	const int32 NumInstances = BuiltData->BuiltInstancesMatrices.Num();
	check(NumInstances > 0);

	BuiltData->InstanceBuffer = MakeUnique<FStaticMeshInstanceData>(false);
	{
		VOXEL_SCOPE_COUNTER("AllocateInstances");
		BuiltData->InstanceBuffer->AllocateInstances(
			NumInstances,
			EResizeBufferFlags::AllowSlackOnGrow | EResizeBufferFlags::AllowSlackOnReduce,
			true);
	}

	{
		VOXEL_SCOPE_COUNTER("SetInstances");
		int32 InstanceIndex = 0;
		for (const auto& Matrix : BuiltData->BuiltInstancesMatrices)
		{
			BuiltData->InstanceBuffer->SetInstance(InstanceIndex++, Matrix.GetCleanMatrix(), Matrix.GetRandomInstanceId());
		}
	}

	// Only check if we're canceled before the BuildTree, as it's the true expensive operation here
	// and if we've finished it, we might as well use the result
	if (CancelCounter->GetValue() > 0)
	{
		return;
	}

	TArray<int32> SortedInstances;
	TArray<int32> InstanceReorderTable;
	{
		VOXEL_SCOPE_COUNTER("BuildTreeAnyThread");
		static_assert(sizeof(FVoxelSpawnerMatrix) == sizeof(FMatrix), "");
		UHierarchicalInstancedStaticMeshComponent::BuildTreeAnyThread(
			// Should be safe
			reinterpret_cast<TArray<FMatrix>&>(BuiltData->BuiltInstancesMatrices),
			MeshBox,
			BuiltData->ClusterTree,
			SortedInstances,
			InstanceReorderTable,
			BuiltData->OcclusionLayerNum,
			DesiredInstancesPerLeaf
#if ENGINE_MINOR_VERSION >= 23
			, false
#endif
		);
	}

	{
		VOXEL_SCOPE_COUNTER("Build Reorder Table");
		BuiltData->InstancesToBuiltInstances = InstanceReorderTable;
		BuiltData->BuiltInstancesToInstances.Init(-1, InstanceReorderTable.Num());
		for (int32 Index = 0; Index < InstanceReorderTable.Num(); Index++)
		{
			BuiltData->BuiltInstancesToInstances[InstanceReorderTable[Index]] = Index;
		}
		for (int32 Index : BuiltData->BuiltInstancesToInstances) check(Index != -1);
	}

	// in-place sort the instances
	{
		VOXEL_SCOPE_COUNTER("Sort Instances");
		for (int32 FirstUnfixedIndex = 0; FirstUnfixedIndex < NumInstances; FirstUnfixedIndex++)
		{
			const int32 LoadFrom = SortedInstances[FirstUnfixedIndex];
			if (LoadFrom != FirstUnfixedIndex)
			{
				check(LoadFrom > FirstUnfixedIndex);
				BuiltData->InstanceBuffer->SwapInstance(FirstUnfixedIndex, LoadFrom);
				// Also keep up to date the transforms array
				BuiltData->BuiltInstancesMatrices.Swap(FirstUnfixedIndex, LoadFrom);
				const int32 SwapGoesTo = InstanceReorderTable[FirstUnfixedIndex];
				check(SwapGoesTo > FirstUnfixedIndex);
				check(SortedInstances[SwapGoesTo] == FirstUnfixedIndex);
				SortedInstances[SwapGoesTo] = LoadFrom;
				InstanceReorderTable[LoadFrom] = SwapGoesTo;
				InstanceReorderTable[FirstUnfixedIndex] = FirstUnfixedIndex;
				SortedInstances[FirstUnfixedIndex] = FirstUnfixedIndex;
			}
		}
	}

	const double EndTime = FPlatformTime::Seconds();
	if (CVarLogHISMBuildTimes.GetValueOnAnyThread() != 0)
	{
		UE_LOG(
			LogVoxel,
			Log,
			TEXT("Building the HISM culling tree took %2.2fms; Instances: %d"),
			(EndTime - StartTime) * 1000,
			NumInstances);
	}

	auto PinnedInstancedMeshManager = InstancedMeshManager.Pin();
	if (PinnedInstancedMeshManager.IsValid())
	{
		PinnedInstancedMeshManager->HISMBuildTaskCallback(Component, BuiltData);
		FVoxelUtilities::DeleteOnGameThread_AnyThread(PinnedInstancedMeshManager);
	}

#undef CHECK_CANCEL
}

uint32 FVoxelHISMBuildTask::GetPriority() const
{
	return 0;
}