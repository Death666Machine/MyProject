// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "IntBox.h"
#include "VoxelSpawners/VoxelBasicSpawner.h"
#include "VoxelWorldGeneratorPicker.h"
#include "VoxelAssetSpawner.generated.h"

class UVoxelAssetSpawner;
class UVoxelTransformableWorldGenerator;
class FVoxelTransformableWorldGeneratorInstance;
class FVoxelAssetSpawnerProxy;

class VOXEL_API FVoxelAssetSpawnerProxyResult : public FVoxelSpawnerProxyResult
{
public:
	const FVoxelAssetSpawnerProxy& Proxy;
	const TArray<FMatrix> Transforms;
	const TArray<int32> GeneratorsIndices;

	FVoxelAssetSpawnerProxyResult(
		const FVoxelAssetSpawnerProxy& Proxy,
		TArray<FMatrix>&& Transforms,
		TArray<int32>&& GeneratorsIndices);

	//~ Begin FVoxelSpawnerProxyResult override
	virtual void Apply_AnyThread() override;
	virtual void Apply_GameThread() override;
	//~ End FVoxelSpawnerProxyResult override

private:
	TArray<FIntBox> TransientGeneratorsBounds;
};

class VOXEL_API FVoxelAssetSpawnerProxy : public FVoxelBasicSpawnerProxy
{
public:
	const TArray<TVoxelSharedPtr<FVoxelTransformableWorldGeneratorInstance>> Generators;
	const FIntBox GeneratorLocalBounds;
	const int32 Priority;
	const bool bRoundAssetPosition;

	FVoxelAssetSpawnerProxy(UVoxelAssetSpawner* Spawner, FVoxelSpawnerManager& Manager);
	virtual ~FVoxelAssetSpawnerProxy();

	//~ Begin FVoxelSpawnerProxy Interface
	virtual TUniquePtr<FVoxelSpawnerProxyResult> ProcessHits(
		const FIntBox& Bounds,
		const TArray<FVoxelSpawnerHit>& Hits,
		const FVoxelConstDataAccelerator& Accelerator) const override;
	virtual void PostSpawn() override {}
	//~ End FVoxelSpawnerProxy Interface
};

UCLASS()
class VOXEL_API UVoxelAssetSpawner : public UVoxelBasicSpawner
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
		FVoxelTransformableWorldGeneratorPicker Generator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
		FIntBox GeneratorLocalBounds = FIntBox(-25, 25);

	// The voxel world seeds will be sent to the generator.
	// Add the names of the seeds you want to be randomized here
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
		TArray<FName> Seeds;

	// All generators are created at begin play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = 1))
		int32 NumberOfDifferentSeedsToUse = 1;

	// Priority of the spawned assets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
		int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
		bool bRoundAssetPosition = false;

public:
	//~ Begin UVoxelSpawner Interface
	virtual TVoxelSharedRef<FVoxelSpawnerProxy> GetSpawnerProxy(FVoxelSpawnerManager& Manager) override;
	//~ End UVoxelSpawner Interface
};