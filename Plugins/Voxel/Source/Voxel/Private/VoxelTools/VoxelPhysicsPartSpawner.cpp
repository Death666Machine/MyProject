// Copyright 2020 Phyronnaz

#include "VoxelTools/VoxelPhysicsPartSpawner.h"
#include "VoxelData/VoxelDataUtilities.h"
#include "VoxelWorld.h"
#include "VoxelWorldRootComponent.h"

#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelPhysicsPartSpawner_VoxelWorlds::SpawnPart(
	TVoxelSharedPtr<FSimpleDelegate>& OutOnWorldUpdateDone,
	AVoxelWorld* World,
	TVoxelSharedPtr<FVoxelData>&& Data,
	TArray<FVoxelPositionValueMaterial>&& Voxels,
	const FIntVector& PartPosition)
{
	check(Data.IsValid());

	UWorld* GameWorld = World->GetWorld();

	FActorSpawnParameters ActorSpawnParameters;
	ActorSpawnParameters.Owner = World;
	ActorSpawnParameters.bDeferConstruction = true;

	UClass* Class = VoxelWorldClass == nullptr ? AVoxelWorld::StaticClass() : VoxelWorldClass.Get();
	FTransform Transform = World->GetTransform();
	Transform.SetLocation(World->LocalToGlobal(PartPosition));
	AVoxelWorld* NewWorld = GameWorld->SpawnActor<AVoxelWorld>(
		Class,
		Transform,
		ActorSpawnParameters);
	if (!ensure(NewWorld)) return;

	// Can't use the world as a template: this would require the new world to have the same class, which we really don't want if the world is eg a BP
	for (TFieldIterator<UProperty> It(AVoxelWorld::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		auto* Property = *It;
		const FName Name = Property->GetFName();
		if (!Property->HasAnyPropertyFlags(EPropertyFlags::CPF_Transient) &&
			Name != STATIC_FNAME("WorldRoot") &&
			Name != STATIC_FNAME("OnWorldLoaded") &&
			Name != STATIC_FNAME("OnWorldDestroyed"))
		{
			Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(NewWorld), Property->ContainerPtrToValuePtr<void>(World));
		}
	}
	NewWorld->bCreateWorldAutomatically = false;
	NewWorld->FinishSpawning(Transform);

	NewWorld->WorldRoot->BodyInstance.bSimulatePhysics = true;
	NewWorld->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseSimpleAndComplex;

	ConfigureVoxelWorld.ExecuteIfBound(NewWorld);

	if (!ensure(!NewWorld->IsCreated())) return;

	NewWorld->PendingData = Data;
	NewWorld->bCreateWorldAutomatically = false;
	NewWorld->SetOctreeDepth(Data->Depth);
	NewWorld->bEnableUndoRedo = Data->bEnableUndoRedo;
	NewWorld->bEnableMultiplayer = Data->bEnableMultiplayer;
	NewWorld->bCreateGlobalPool = false;

	OutOnWorldUpdateDone = MakeVoxelShared<FSimpleDelegate>();
	OutOnWorldUpdateDone->BindLambda([VoxelWorld = TWeakObjectPtr<AVoxelWorld>(NewWorld)]
		{
			if (VoxelWorld.IsValid() && ensure(!VoxelWorld->IsCreated()))
			{
				VoxelWorld->CreateWorld();
			}
		});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelPhysicsPartSpawner_Cubes::UVoxelPhysicsPartSpawner_Cubes()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Engine/BasicShapes/Cube"));
	CubeMesh = MeshFinder.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Voxel/Examples/Materials/RGB/M_VoxelMaterial_Colors_Parameter"));
	Material = MaterialFinder.Object;
}

void UVoxelPhysicsPartSpawner_Cubes::SpawnPart(
	TVoxelSharedPtr<FSimpleDelegate>& OutOnWorldUpdateDone,
	AVoxelWorld* World,
	TVoxelSharedPtr<FVoxelData>&& Data,
	TArray<FVoxelPositionValueMaterial>&& Voxels,
	const FIntVector& PartPosition)
{
	const FRotator Rotation = FRotator(World->GetTransform().GetRotation());
	for (const auto& Voxel : Voxels)
	{
		auto* StaticMeshActor = GetWorld()->SpawnActor<AStaticMeshActor>(
			World->LocalToGlobal(Voxel.Position + PartPosition),
			Rotation);
		if (!ensure(StaticMeshActor)) continue;

		StaticMeshActor->SetActorScale3D(FVector(World->VoxelSize / 100));
		StaticMeshActor->SetMobility(EComponentMobility::Movable);

		UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();
		if (!ensure(StaticMeshComponent)) continue;

		StaticMeshComponent->SetStaticMesh(CubeMesh);
		StaticMeshComponent->SetSimulatePhysics(false); // False until world is updated
		{
			auto* Instance = UMaterialInstanceDynamic::Create(Material, StaticMeshActor);
			Instance->SetVectorParameterValue(STATIC_FNAME("VertexColor"), Voxel.Material.GetLinearColor());
			StaticMeshComponent->SetMaterial(0, Instance);
		}
		Cubes.Add(StaticMeshActor);
	}

	OutOnWorldUpdateDone = MakeVoxelShared<FSimpleDelegate>();
	OutOnWorldUpdateDone->BindLambda([Cubes = TArray<TWeakObjectPtr<AStaticMeshActor>>(Cubes)]
		{
			for (auto& Cube : Cubes)
			{
				if (Cube.IsValid())
				{
					UStaticMeshComponent* StaticMeshComponent = Cube->GetStaticMeshComponent();
					if (!ensure(StaticMeshComponent)) continue;
					StaticMeshComponent->SetSimulatePhysics(true);
				}
			}
		});
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelPhysicsPartSpawner_GetVoxels::SpawnPart(
	TVoxelSharedPtr<FSimpleDelegate>& OutOnWorldUpdateDone,
	AVoxelWorld* World,
	TVoxelSharedPtr<FVoxelData>&& Data,
	TArray<FVoxelPositionValueMaterial>&& InVoxels,
	const FIntVector& PartPosition)
{
	Voxels.Emplace(FVoxelPositionValueMaterialArray{ MoveTemp(InVoxels) });
}