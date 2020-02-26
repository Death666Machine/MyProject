// Copyright 2020 Phyronnaz

#include "VoxelSpawners/VoxelInstancedMeshManager.h"
#include "VoxelSpawners/VoxelHierarchicalInstancedStaticMeshComponent.h"
#include "VoxelSpawners/VoxelActor.h"
#include "VoxelProcGen/VoxelProcGenManager.h"
#include "VoxelData/VoxelDataAccelerator.h"
#include "VoxelGlobals.h"
#include "VoxelWorld.h"

#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("InstancedMeshManager Num Spawned Actors"), STAT_FVoxelInstancedMeshManager_NumSpawnedActors, STATGROUP_Voxel);

FVoxelInstancedMeshManagerSettings::FVoxelInstancedMeshManagerSettings(
	const AVoxelWorld* World,
	const TVoxelSharedRef<IVoxelPool>& Pool,
	const TVoxelSharedRef<FVoxelProcGenManager>& ProcGenManager)
	: ComponentsOwner(const_cast<AVoxelWorld*>(World))
	, NumberOfInstancesPerHISM(World->NumberOfInstancesPerHISM)
	, WorldOffset(World->GetWorldOffsetPtr())
	, Pool(Pool)
	, ProcGenManager(ProcGenManager)
	, CollisionDistanceInChunks(FMath::Max(0, World->SpawnersCollisionDistanceInVoxel / int32(CollisionChunkSize)))
	, VoxelSize(World->VoxelSize)
{
}

FVoxelInstancedMeshManager::FVoxelInstancedMeshManager(const FVoxelInstancedMeshManagerSettings& Settings)
	: Settings(Settings)
{
}

TVoxelSharedRef<FVoxelInstancedMeshManager> FVoxelInstancedMeshManager::Create(const FVoxelInstancedMeshManagerSettings& Settings)
{
	return MakeShareable(new FVoxelInstancedMeshManager(Settings));
}

void FVoxelInstancedMeshManager::Destroy()
{
	StopTicking();
}

FVoxelInstancedMeshManager::~FVoxelInstancedMeshManager()
{
	ensure(IsInGameThread());
}

void FVoxelInstancedMeshManager::AddInstances(const FVoxelInstancedMeshSettings& MeshSettings, const TArray<FVoxelSpawnerMatrix>& Transforms, const FIntBox& Bounds)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(Transforms.Num() > 0)) return;

	UVoxelHierarchicalInstancedStaticMeshComponent* HISM;
	auto& Components = MeshMap.FindOrAdd(MeshSettings);
	if (Components.Num() == 0 ||
		!Components.Last().IsValid() ||
		Components.Last()->Voxel_GetNumInstances() + Transforms.Num() > Settings.NumberOfInstancesPerHISM)
	{
		HISM = CreateHISM(MeshSettings);
		Components.Add(HISM);
	}
	else
	{
		HISM = Components.Last().Get();
	}
	if (HISM)
	{
		HISM->Voxel_AppendTransforms(Transforms, Bounds);
	}
}

AVoxelSpawnerActor* FVoxelInstancedMeshManager::SpawnActor(TSubclassOf<AVoxelSpawnerActor> ActorTemplate, TWeakObjectPtr<UStaticMesh> Mesh, FVoxelSpawnerMatrix Matrix) const
{
	VOXEL_FUNCTION_COUNTER();
	INC_DWORD_STAT(STAT_FVoxelInstancedMeshManager_NumSpawnedActors);

	if (!ActorTemplate)
	{
		return nullptr;
	}

	auto* ComponentsOwner = Settings.ComponentsOwner.Get();
	if (!ComponentsOwner)
	{
		return nullptr;
	}
	auto* World = ComponentsOwner->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview)
	{
		return nullptr;
	}

	const float InstanceRandom = Matrix.GetRandomInstanceId();
	const FTransform LocalTransform(Matrix.GetCleanMatrix().ConcatTranslation(FVector(*Settings.WorldOffset) * Settings.VoxelSize));
	FTransform GlobalTransform = LocalTransform * ComponentsOwner->GetTransform();

	auto* Actor = World->SpawnActor(ActorTemplate, &GlobalTransform);
	if (!Actor || !ensure(Actor->IsA<AVoxelSpawnerActor>()))
	{
		return nullptr;
	}

	auto* VoxelSpawnerActor = CastChecked<AVoxelSpawnerActor>(Actor);
	VoxelSpawnerActor->SetStaticMesh(Mesh.Get());
	VoxelSpawnerActor->SetInstanceRandom(InstanceRandom);
	return VoxelSpawnerActor;
}

void FVoxelInstancedMeshManager::SpawnActors(
	TSubclassOf<AVoxelSpawnerActor> ActorTemplate,
	TWeakObjectPtr<UStaticMesh> Mesh,
	const TArray<FVoxelSpawnerMatrix>& Transforms) const
{
	VOXEL_FUNCTION_COUNTER();

	for (auto& Transform : Transforms)
	{
		if (!SpawnActor(ActorTemplate, Mesh, Transform))
		{
			return;
		}
	}
}

TArray<AVoxelSpawnerActor*> FVoxelInstancedMeshManager::SpawnActorsInArea(const FIntBox& Bounds, const FVoxelData& Data, EVoxelSpawnerActorSpawnType SpawnType) const
{
	VOXEL_FUNCTION_COUNTER();

	const auto TransformsMap = RemoveActorsInArea(Bounds, Data, SpawnType);

	TArray<AVoxelSpawnerActor*> Actors;
	for (auto& It : TransformsMap)
	{
		SpawnActors(It.Key.ActorTemplate, It.Key.Mesh, It.Value);
	}

	return Actors;
}

TMap<FVoxelInstancedMeshSettings, TArray<FVoxelSpawnerMatrix>> FVoxelInstancedMeshManager::RemoveActorsInArea(
	const FIntBox& Bounds,
	const FVoxelData& Data,
	EVoxelSpawnerActorSpawnType SpawnType) const
{
	VOXEL_FUNCTION_COUNTER();

	const auto ExtendedBounds = Bounds.Extend(1); // As we are accessing floats, they can be between Max - 1 and Max

	TMap<FVoxelInstancedMeshSettings, TArray<FVoxelSpawnerMatrix>> TransformsMap;

	FVoxelReadScopeLock Lock(Data, ExtendedBounds, "SpawnActorsInArea");

	const TUniquePtr<FVoxelConstDataAccelerator> Accelerator =
		SpawnType == EVoxelSpawnerActorSpawnType::All
		? nullptr
		: MakeUnique<FVoxelConstDataAccelerator>(Data, ExtendedBounds);

	for (auto& It : MeshMap)
	{
		auto& Transforms = TransformsMap.FindOrAdd(It.Key);
		for (auto& HISM : It.Value)
		{
			if (HISM.IsValid())
			{
				HISM->Voxel_RemoveMeshesInArea(Bounds, Accelerator.Get(), SpawnType, Transforms);
			}
		}
	}

	return TransformsMap;
}

void FVoxelInstancedMeshManager::RecomputeMeshPositions()
{
	VOXEL_FUNCTION_COUNTER();

	for (auto& It : MeshMap)
	{
		for (auto& HISM : It.Value)
		{
			SetHISMRelativeLocation(*HISM);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelInstancedMeshManager::Tick(float DeltaTime)
{
	VOXEL_FUNCTION_COUNTER();

	FQueuedBuildCallback Callback;
	while (HISMBuiltDataQueue.Dequeue(Callback))
	{
		auto* HISM = Callback.Component.Get();
		if (!HISM) continue;

		HISM->Voxel_FinishBuilding(*Callback.Data);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelInstancedMeshManager::HISMBuildTaskCallback(
	TWeakObjectPtr<UVoxelHierarchicalInstancedStaticMeshComponent> Component,
	const TVoxelSharedRef<FVoxelHISMBuiltData>& BuiltData)
{
	HISMBuiltDataQueue.Enqueue({ Component, BuiltData });
}

UVoxelHierarchicalInstancedStaticMeshComponent* FVoxelInstancedMeshManager::CreateHISM(const FVoxelInstancedMeshSettings& MeshSettings) const
{
	VOXEL_FUNCTION_COUNTER();

	UE_LOG(LogVoxel, Log, TEXT("Creating a new HISM for mesh %s"), *MeshSettings.Mesh->GetPathName());

	auto* ComponentsOwner = Settings.ComponentsOwner.Get();
	if (!ComponentsOwner)
	{
		return nullptr;
	}

	UVoxelHierarchicalInstancedStaticMeshComponent* HISM;
	if (MeshSettings.HISMTemplate)
	{
		HISM = NewObject<UVoxelHierarchicalInstancedStaticMeshComponent>(ComponentsOwner, MeshSettings.HISMTemplate.Get(), NAME_None, RF_Transient);
	}
	else
	{
		HISM = NewObject<UVoxelHierarchicalInstancedStaticMeshComponent>(ComponentsOwner, NAME_None, RF_Transient);
	}

	HISM->Init(
		Settings.Pool,
		const_cast<FVoxelInstancedMeshManager&>(*this).AsShared(),
		Settings.VoxelSize);
	HISM->Voxel_BuildDelay = MeshSettings.BuildDelay;
	HISM->bDisableCollision = true;

#define EQ(X) HISM->X = MeshSettings.X;
	EQ(bAffectDynamicIndirectLighting)
		EQ(bAffectDistanceFieldLighting)
		EQ(bCastShadowAsTwoSided)
		EQ(bReceivesDecals)
		EQ(bUseAsOccluder)
		EQ(BodyInstance)
		EQ(LightingChannels)
		EQ(bRenderCustomDepth)
		EQ(CustomDepthStencilValue)
#undef EQ
		HISM->InstanceStartCullDistance = MeshSettings.CullDistance.Min;
	HISM->InstanceEndCullDistance = MeshSettings.CullDistance.Max;
	HISM->CastShadow = MeshSettings.bCastShadow;
	HISM->bCastDynamicShadow = MeshSettings.bCastShadow;
	HISM->BodyInstance = MeshSettings.BodyInstance;
	HISM->SetCustomNavigableGeometry(MeshSettings.CustomNavigableGeometry);
	HISM->SetStaticMesh(MeshSettings.Mesh.Get());
	HISM->SetupAttachment(ComponentsOwner->GetRootComponent(), NAME_None);
	HISM->RegisterComponent();

	SetHISMRelativeLocation(*HISM);

	if (MeshSettings.BodyInstance.GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		Settings.ProcGenManager->BindEvent(
			true,
			Settings.CollisionChunkSize,
			Settings.CollisionDistanceInChunks,
			FChunkDelegate::CreateUObject(HISM, &UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_EnablePhysics),
			FChunkDelegate::CreateUObject(HISM, &UVoxelHierarchicalInstancedStaticMeshComponent::Voxel_DisablePhysics));
	}

	return HISM;
}

void FVoxelInstancedMeshManager::SetHISMRelativeLocation(UVoxelHierarchicalInstancedStaticMeshComponent& HISM) const
{
	VOXEL_FUNCTION_COUNTER();

	const FIntVector Position = { 0, 0, 0 };
	HISM.SetRelativeLocation(FVector(Position + *Settings.WorldOffset) * Settings.VoxelSize, false, nullptr, ETeleportType::TeleportPhysics);
}