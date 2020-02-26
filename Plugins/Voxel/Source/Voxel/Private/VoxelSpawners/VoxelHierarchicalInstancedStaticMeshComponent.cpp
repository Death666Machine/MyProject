// Copyright 2020 Phyronnaz

#include "VoxelSpawners/VoxelHierarchicalInstancedStaticMeshComponent.h"
#include "VoxelSpawners/VoxelHISMBuildTask.h"
#include "VoxelDebug/VoxelDebugManager.h"
#include "VoxelData/VoxelDataUtilities.h"
#include "VoxelData/VoxelDataAccelerator.h"
#include "VoxelGlobals.h"
#include "IVoxelPool.h"

#include "Async/Async.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/Private/InstancedStaticMesh.h"

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Voxel HISM Num Instances"), STAT_VoxelHISMComponent_NumInstances, STATGROUP_VoxelMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Voxel HISM Num Physics Bodies"), STAT_VoxelHISMComponent_NumPhysicsBodies, STATGROUP_VoxelMemory);
DECLARE_DWORD_COUNTER_STAT(TEXT("Voxel HISM Num Floating Mesh Checked"), STAT_VoxelHISMComponent_NumFloatingMeshChecked, STATGROUP_Voxel);
DECLARE_MEMORY_STAT(TEXT("Voxel HISM Memory"), STAT_VoxelHISMMemory, STATGROUP_VoxelMemory);

static TAutoConsoleVariable<int32> CVarShowHISMCollisions(
	TEXT("voxel.spawners.ShowCollisions"),
	0,
	TEXT("If true, will show a debug point on HISM instances with collisions"),
	ECVF_Default);

static const FMatrix EmptyMatrix = FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector).ToMatrixWithScale();

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelHierarchicalInstancedStaticMeshComponent::UVoxelHierarchicalInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	PrimaryComponentTick.bCanEverTick = true;
#endif

	UpdateAllocatedMemory();
}

UVoxelHierarchicalInstancedStaticMeshComponent::~UVoxelHierarchicalInstancedStaticMeshComponent()
{
	DEC_MEMORY_STAT_BY(STAT_VoxelHISMMemory, Voxel_AllocatedMemory);
	DEC_DWORD_STAT_BY(STAT_VoxelHISMComponent_NumInstances, Voxel_UnbuiltMatrices.Num());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelHierarchicalInstancedStaticMeshComponent::Init(
	TVoxelWeakPtr<IVoxelPool> Pool,
	TVoxelWeakPtr<FVoxelInstancedMeshManager> InstancedMeshManager,
	float VoxelSize)
{
	Voxel_Pool = Pool;
	Voxel_InstancedMeshManager = InstancedMeshManager;
	Voxel_VoxelSize = VoxelSize;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_AppendTransforms(const TArray<FVoxelSpawnerMatrix>& InTransforms, const FIntBox& InBounds)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(InTransforms.Num() > 0)) return;

	INC_DWORD_STAT_BY(STAT_VoxelHISMComponent_NumInstances, InTransforms.Num());

	Voxel_UnbuiltMatrices.Append(InTransforms);
	Voxel_PendingNewInstancesBounds.Add(InBounds);

	if (Voxel_BuildDelay <= 0)
	{
		Voxel_StartBuildTree();
	}
	else
	{
		auto& TimerManager = GetWorld()->GetTimerManager();
		TimerManager.SetTimer(
			Voxel_TimerHandle,
			this,
			&UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_StartBuildTree,
			Voxel_BuildDelay,
			false);
	}

	UpdateAllocatedMemory();
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_StartBuildTree()
{
	VOXEL_FUNCTION_COUNTER();

	ensure((Voxel_TaskUniqueId != 0) == Voxel_TaskCancelCounterPtr.IsValid());

	if (!GetStaticMesh()) return;
	if (!ensure(Voxel_UnbuiltMatrices.Num() > 0)) return;

	auto Pool = Voxel_Pool.Pin();
	if (!ensure(Pool.IsValid())) return;

	if (Voxel_TaskCancelCounterPtr.IsValid())
	{
		Voxel_TaskCancelCounterPtr->Increment();
		Voxel_TaskCancelCounterPtr.Reset();
	}

	auto* Task = new FVoxelHISMBuildTask(this, Voxel_UnbuiltMatrices);
	Voxel_TaskUniqueId = Task->UniqueId;
	Voxel_TaskCancelCounterPtr = Task->CancelCounter;
	Pool->QueueTask(EVoxelTaskType::HISMBuild, Task);

	ensure(!Voxel_TaskIdToNewInstancesBounds.Contains(Voxel_TaskUniqueId));
	ensure(Voxel_PendingNewInstancesBounds.Num() > 0);
	Voxel_TaskIdToNewInstancesBounds.Add(Voxel_TaskUniqueId, MoveTemp(Voxel_PendingNewInstancesBounds));

	UpdateAllocatedMemory();
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_FinishBuilding(FVoxelHISMBuiltData& BuiltData)
{
	VOXEL_FUNCTION_COUNTER();

	ensure((Voxel_TaskUniqueId != 0) == Voxel_TaskCancelCounterPtr.IsValid());

	if (Voxel_TaskUniqueId == 0)
	{
		// Task is too late, newer one already completed
		ensure(!Voxel_TaskIdToNewInstancesBounds.Contains(BuiltData.UniqueId));
		ensure(Voxel_UnbuiltInstancesToClear.Num() == 0);
		return;
	}

	ensure(Voxel_TaskUniqueId >= BuiltData.UniqueId); // Else the task time traveled
	const bool bIsLatestTask = BuiltData.UniqueId == Voxel_TaskUniqueId;

	for (int32 UnbuiltIndex : Voxel_UnbuiltInstancesToClear)
	{
		if (!BuiltData.InstancesToBuiltInstances.IsValidIndex(UnbuiltIndex))
		{
			ensure(!bIsLatestTask);
			continue;
		}
		const int32 BuiltIndex = BuiltData.InstancesToBuiltInstances[UnbuiltIndex];
		BuiltData.InstanceBuffer->SetInstance(BuiltIndex, EmptyMatrix, 0);
		BuiltData.BuiltInstancesMatrices[BuiltIndex] = FVoxelSpawnerMatrix(EmptyMatrix);
	}

	if (bIsLatestTask)
	{
		Voxel_TaskUniqueId = 0;
		Voxel_TaskCancelCounterPtr.Reset();
		Voxel_UnbuiltInstancesToClear.Reset();
	}
	else
	{
		// Do not reset the task id nor cancel it as this is not the right one yet
	}

	const int32 NumInstances = BuiltData.InstanceBuffer->GetNumInstances();
	check(NumInstances > 0);
	check(NumInstances == BuiltData.BuiltInstancesMatrices.Num());
	check(NumInstances == BuiltData.InstancesToBuiltInstances.Num());
	check(NumInstances == BuiltData.BuiltInstancesToInstances.Num());
	// Not true with timer delays ensure(!bIsLatestTask || NumInstances == Voxel_UnbuiltMatrices.Num());

	Voxel_BuiltMatrices = MoveTemp(BuiltData.BuiltInstancesMatrices);

	Voxel_InstancesToBuiltInstances = MoveTemp(BuiltData.InstancesToBuiltInstances);
	Voxel_BuiltInstancesToInstances = MoveTemp(BuiltData.BuiltInstancesToInstances);

	constexpr bool bRequireCPUAccess = true;
	if (!PerInstanceRenderData.IsValid() || PerInstanceRenderData->InstanceBuffer.RequireCPUAccess != bRequireCPUAccess)
	{
		if (ensure(PerInstanceRenderData.IsValid()))
		{
			VOXEL_SCOPE_COUNTER("ReleasePerInstanceRenderData");
			ReleasePerInstanceRenderData();
		}

		VOXEL_SCOPE_COUNTER("InitPerInstanceRenderData");
		InitPerInstanceRenderData(true, BuiltData.InstanceBuffer.Get(), bRequireCPUAccess);
	}
	else
	{
		VOXEL_SCOPE_COUNTER("UpdateFromPreallocatedData");
		PerInstanceRenderData->UpdateFromPreallocatedData(*BuiltData.InstanceBuffer);
	}

	{
		VOXEL_SCOPE_COUNTER("AcceptPrebuiltTree");
		AcceptPrebuiltTree(BuiltData.ClusterTree, BuiltData.OcclusionLayerNum, NumInstances);
	}

	for (auto It = Voxel_TaskIdToNewInstancesBounds.CreateIterator(); It; ++It)
	{
		if (It.Key() <= BuiltData.UniqueId)
		{
			for (auto& Chunk : It.Value())
			{
				Voxel_RefreshPhysics(Chunk);
			}
			It.RemoveCurrent();
		}
	}

	UpdateAllocatedMemory();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_EnablePhysics(const FIntBox Chunk)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(!Voxel_InstanceBodies.Contains(Chunk))) return;
	auto& Bodies = Voxel_InstanceBodies.Add(Chunk);

	Voxel_EnablePhysicsImpl(Chunk, Bodies);

	UpdateAllocatedMemory();
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_DisablePhysics(const FIntBox Chunk)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(Voxel_InstanceBodies.Contains(Chunk))) return;
	auto Bodies = Voxel_InstanceBodies.FindAndRemoveChecked(Chunk);

	Voxel_DisablePhysicsImpl(Bodies);

	UpdateAllocatedMemory();
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_RefreshPhysics(const FIntBox& BoundsToUpdate)
{
	VOXEL_FUNCTION_COUNTER();

	for (auto& It : Voxel_InstanceBodies)
	{
		const auto& Chunk = It.Key;
		auto& Bodies = It.Value;
		if (Chunk.Intersect(BoundsToUpdate))
		{
			Voxel_DisablePhysicsImpl(Bodies);
			ensure(Bodies.Num() == 0);
			Voxel_EnablePhysicsImpl(Chunk, Bodies);
		}
	}

	UpdateAllocatedMemory();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_RemoveMeshesInArea(
	const FIntBox& VoxelBounds,
	const FVoxelConstDataAccelerator* Accelerator,
	EVoxelSpawnerActorSpawnType SpawnType,
	TArray<FVoxelSpawnerMatrix>& OutMatrices)
{
	VOXEL_FUNCTION_COUNTER();

	check(SpawnType == EVoxelSpawnerActorSpawnType::All || Accelerator);

	const auto ScaledBounds = VoxelBounds.Scale(Voxel_VoxelSize);

	TArray<int32> BuiltIndicesToClear;
	FIntBoxWithValidity BoundsToUpdate;

	const auto Lambda = [&](const int32 BuiltIndex)
	{
		INC_DWORD_STAT_BY(STAT_VoxelHISMComponent_NumFloatingMeshChecked, 1);

		const FVoxelSpawnerMatrix Matrix = Voxel_BuiltMatrices[BuiltIndex];
		const FTransform LocalInstanceTransform = FTransform(Matrix.GetCleanMatrix());
		// Remove position offset to be on the voxel surface
		const FVector VoxelPosition = (LocalInstanceTransform.GetTranslation() - Matrix.GetPositionOffset()) / Voxel_VoxelSize;

		if (LocalInstanceTransform.GetScale3D().IsNearlyZero() ||
			!VoxelBounds.Contains(FIntBox(VoxelPosition)))
		{
			return;
		}

		if (SpawnType == EVoxelSpawnerActorSpawnType::All || Accelerator->GetFloatValue(VoxelPosition, 0) > 0)
		{
			OutMatrices.Add(Matrix);
			BuiltIndicesToClear.Add(BuiltIndex);
			BoundsToUpdate += FIntBox(VoxelPosition);
		}
	};
	if (!ensure(ClusterTreePtr.IsValid())) return;
	Voxel_IterateInstancesInBounds(*ClusterTreePtr, ScaledBounds, Lambda);

	if (BuiltIndicesToClear.Num() == 0)
	{
		return;
	}

	const auto InstanceBuffer = PerInstanceRenderData.IsValid() ? PerInstanceRenderData->InstanceBuffer_GameThread : nullptr;
	ensure(InstanceBuffer.IsValid() || Voxel_TaskUniqueId > 0);

	for (int32 BuiltIndex : BuiltIndicesToClear)
	{
		const int32 UnbuiltIndex = Voxel_BuiltInstancesToInstances[BuiltIndex];

		if (InstanceBuffer.IsValid())
		{
			InstanceBuffer->SetInstance(BuiltIndex, EmptyMatrix, 0);
		}
		ensure(Voxel_BuiltMatrices[BuiltIndex] == Voxel_UnbuiltMatrices[UnbuiltIndex]);
		Voxel_BuiltMatrices[BuiltIndex] = FVoxelSpawnerMatrix(EmptyMatrix);
		Voxel_UnbuiltMatrices[UnbuiltIndex] = FVoxelSpawnerMatrix(EmptyMatrix);

		if (Voxel_TaskUniqueId > 0)
		{
			Voxel_UnbuiltInstancesToClear.Add(UnbuiltIndex);
		}
	}

	if (InstanceBuffer.IsValid())
	{
		ENQUEUE_RENDER_COMMAND(UVoxelHierarchicalInstancedStaticMeshComponent_UpdateBuffer)(
			[PerInstanceRenderData = PerInstanceRenderData](FRHICommandListImmediate& RHICmdList)
		{
			PerInstanceRenderData->InstanceBuffer.InstanceData = PerInstanceRenderData->InstanceBuffer_GameThread;
			PerInstanceRenderData->InstanceBuffer.UpdateRHI();
		}
		);
		MarkRenderStateDirty();
	}

	Voxel_RefreshPhysics(BoundsToUpdate.GetBox());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelHierarchicalInstancedStaticMeshComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	VOXEL_FUNCTION_COUNTER();

	ensure((Voxel_TaskUniqueId != 0) == Voxel_TaskCancelCounterPtr.IsValid());

	Voxel_TaskUniqueId = 0;
	if (Voxel_TaskCancelCounterPtr.IsValid())
	{
		Voxel_TaskCancelCounterPtr->Increment();
		Voxel_TaskCancelCounterPtr.Reset();
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UVoxelHierarchicalInstancedStaticMeshComponent::OnDestroyPhysicsState()
{
	VOXEL_FUNCTION_COUNTER();

	Super::OnDestroyPhysicsState();

	for (auto& It : Voxel_InstanceBodies)
	{
		Voxel_DisablePhysicsImpl(It.Value);
	}
	Voxel_InstanceBodies.Empty();
}

void UVoxelHierarchicalInstancedStaticMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	VOXEL_FUNCTION_COUNTER();

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CVarShowHISMCollisions.GetValueOnGameThread())
	{
		if (!GetStaticMesh()) return;

		const FBox MeshBox = GetStaticMesh()->GetBounds().GetBox();
		for (auto& It : Voxel_InstanceBodies)
		{
			for (auto* Body : It.Value)
			{
				const FBox BodyBox = MeshBox.TransformBy(Body->GetUnrealWorldTransform());
				DrawDebugBox(
					GetWorld(),
					BodyBox.GetCenter(),
					BodyBox.GetExtent(),
					FColor::Red,
					false,
					DeltaTime * 2);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelHierarchicalInstancedStaticMeshComponent::UpdateAllocatedMemory()
{
	VOXEL_FUNCTION_COUNTER();

	DEC_MEMORY_STAT_BY(STAT_VoxelHISMMemory, Voxel_AllocatedMemory);

	Voxel_AllocatedMemory = 0;
	Voxel_AllocatedMemory += Voxel_UnbuiltMatrices.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_BuiltMatrices.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_InstancesToBuiltInstances.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_BuiltInstancesToInstances.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_UnbuiltInstancesToClear.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_TaskIdToNewInstancesBounds.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_PendingNewInstancesBounds.GetAllocatedSize();
	Voxel_AllocatedMemory += Voxel_InstanceBodies.GetAllocatedSize();

	INC_MEMORY_STAT_BY(STAT_VoxelHISMMemory, Voxel_AllocatedMemory);
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_EnablePhysicsImpl(const FIntBox& Chunk, TArray<FBodyInstance*>& OutBodies) const
{
	VOXEL_FUNCTION_COUNTER();

	// We want this function to be const
	auto* const Component = const_cast<UPrimitiveComponent*>(static_cast<const UPrimitiveComponent*>(this));

	UBodySetup* const BodySetup = Component->GetBodySetup();
	if (!ensure(BodySetup)) return;

	const auto ScaledBounds = Chunk.Scale(Voxel_VoxelSize);

	TArray<FTransform> Transforms;
	const auto Lambda = [&](const int32 BuiltIndex)
	{
		const FTransform LocalInstanceTransform = FTransform(Voxel_BuiltMatrices[BuiltIndex].GetCleanMatrix());
		const FTransform GlobalInstanceTransform = LocalInstanceTransform * GetComponentTransform();

		if (GlobalInstanceTransform.GetScale3D().IsNearlyZero() ||
			!ScaledBounds.ContainsFloat(LocalInstanceTransform.GetTranslation()))
		{
			return;
		}

		FBodyInstance* Instance = new FBodyInstance();

		Instance->CopyBodyInstancePropertiesFrom(&BodyInstance);
		Instance->bAutoWeld = false;

		// Make sure we never enable bSimulatePhysics for ISMComps
		Instance->bSimulatePhysics = false;

		// Set the body index to the UNBUILT index
		Instance->InstanceBodyIndex = Voxel_BuiltInstancesToInstances[BuiltIndex];
		ensure(Voxel_UnbuiltMatrices[Instance->InstanceBodyIndex] == Voxel_BuiltMatrices[BuiltIndex]);

		OutBodies.Add(Instance);
		Transforms.Add(GlobalInstanceTransform);
	};

	if (!ensure(ClusterTreePtr.IsValid())) return;
	Voxel_IterateInstancesInBounds(*ClusterTreePtr, ScaledBounds, Lambda);

	if (OutBodies.Num() == 0) return;

	INC_DWORD_STAT_BY(STAT_VoxelHISMComponent_NumPhysicsBodies, OutBodies.Num());
	VOXEL_SCOPE_COUNTER("InitStaticBodies");
	FBodyInstance::InitStaticBodies(OutBodies, Transforms, BodySetup, Component, GetWorld()->GetPhysicsScene());
}

void UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_DisablePhysicsImpl(TArray<FBodyInstance*>& Bodies) const
{
	VOXEL_FUNCTION_COUNTER();

	DEC_DWORD_STAT_BY(STAT_VoxelHISMComponent_NumPhysicsBodies, Bodies.Num());

	for (auto* Body : Bodies)
	{
		Body->TermBody();
		delete Body;
	}
	Bodies.Reset();
}