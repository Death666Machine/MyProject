// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelSpawners/VoxelBasicSpawner.h"
#include "VoxelSpawners/VoxelActor.h"
#include "VoxelSpawners/VoxelInstancedMeshSettings.h"
#include "VoxelSpawners/VoxelSpawnerMatrix.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "VoxelMeshSpawner.generated.h"

class FVoxelConstDataAccelerator;
class FVoxelMeshSpawnerProxy;
class FVoxelMeshSpawnerGroupProxy;
class UStaticMesh;
class UVoxelMeshSpawnerBase;
class UVoxelMeshSpawnerGroup;
class UVoxelHierarchicalInstancedStaticMeshComponent;

class VOXEL_API FVoxelMeshSpawnerProxyResult : public FVoxelSpawnerProxyResult
{
public:
	const FVoxelMeshSpawnerProxy& Proxy;
	const FIntBox Bounds;
	const TArray<FVoxelSpawnerMatrix> Matrices;

	FVoxelMeshSpawnerProxyResult(
		const FVoxelMeshSpawnerProxy& Proxy,
		const FIntBox& Bounds,
		TArray<FVoxelSpawnerMatrix>&& Matrices);

	//~ Begin FVoxelSpawnerProxyResult override
	virtual void Apply_GameThread() override;
	//~ End FVoxelSpawnerProxyResult override
};

class VOXEL_API FVoxelMeshSpawnerProxy : public FVoxelBasicSpawnerProxy
{
public:
	const FVoxelInstancedMeshSettings InstanceSettings;
	const bool bAlwaysSpawnActor;
	const bool bSendVoxelMaterialThroughInstanceRandom;
	const FVector FloatingDetectionOffset;

	FVoxelMeshSpawnerProxy(UVoxelMeshSpawnerBase* Spawner, TWeakObjectPtr<UStaticMesh> Mesh, FVoxelSpawnerManager& Manager, uint32 Seed = 0);

	//~ Begin FVoxelSpawnerProxy Interface
	virtual TUniquePtr<FVoxelSpawnerProxyResult> ProcessHits(
		const FIntBox& Bounds,
		const TArray<FVoxelSpawnerHit>& Hits,
		const FVoxelConstDataAccelerator& Accelerator) const override;
	virtual void PostSpawn() override {}
	//~ End FVoxelSpawnerProxy Interface
};

class VOXEL_API FVoxelMeshSpawnerGroupProxy : public FVoxelSpawnerProxy
{
public:
	const TArray<TVoxelSharedPtr<FVoxelMeshSpawnerProxy>> Proxies;

	FVoxelMeshSpawnerGroupProxy(UVoxelMeshSpawnerGroup* Spawner, FVoxelSpawnerManager& Manager);

	//~ Begin FVoxelSpawnerProxy Interface
	virtual TUniquePtr<FVoxelSpawnerProxyResult> ProcessHits(
		const FIntBox& Bounds,
		const TArray<FVoxelSpawnerHit>& Hits,
		const FVoxelConstDataAccelerator& Accelerator) const override;
	virtual void PostSpawn() override {}
	//~ End FVoxelSpawnerProxy Interface
};

UCLASS(Abstract)
class VOXEL_API UVoxelMeshSpawnerBase : public UVoxelBasicSpawner
{
	GENERATED_BODY()

public:
	// Actor to spawn when enabling physics. After spawn, the SetStaticMesh event will be called on the actor with Mesh as argument
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
		TSubclassOf<AVoxelSpawnerActor> ActorTemplate = AVoxelSpawnerActorWithStaticMeshAndAutoDisable::StaticClass();

	// Will always spawn an actor instead of an instanced mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
		bool bAlwaysSpawnActor = false;

public:
	// If true, the voxel material will be sent through PerInstanceRandom, allowing to eg set the instance color to the material color
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance Settings")
		bool bSendVoxelMaterialThroughInstanceRandom = false;

	UPROPERTY(EditAnywhere, Category = "Instance Settings", meta = (ShowOnlyInnerProperties))
		FVoxelInstancedMeshSettings InstancedMeshSettings;

public:
	// In cm. Increase this if your foliage is enabling physics too soon
	UPROPERTY(EditAnywhere, Category = "Placement - Offset")
		FVector FloatingDetectionOffset = FVector(0, 0, -10);

protected:
	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface
};

UCLASS()
class VOXEL_API UVoxelMeshSpawner : public UVoxelMeshSpawnerBase
{
	GENERATED_BODY()

public:
	// Mesh to spawn. Can be left to null if AlwaysSpawnActor is true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
		UStaticMesh* Mesh = nullptr;

public:
	//~ Begin UVoxelSpawner Interface
	virtual TVoxelSharedRef<FVoxelSpawnerProxy> GetSpawnerProxy(FVoxelSpawnerManager& Manager) override;
	//~ End UVoxelSpawner Interface
};

UCLASS()
class VOXEL_API UVoxelMeshSpawnerGroup : public UVoxelMeshSpawnerBase
{
	GENERATED_BODY()

public:
	// Meshes to spawn. Can be left to null if AlwaysSpawnActor is true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
		TArray<UStaticMesh*> Meshes;

public:
	//~ Begin UVoxelSpawner Interface
	virtual TVoxelSharedRef<FVoxelSpawnerProxy> GetSpawnerProxy(FVoxelSpawnerManager& Manager) override;
	virtual float GetDistanceBetweenInstancesInVoxel() const override;
	//~ End UVoxelSpawner Interface
};