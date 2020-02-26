// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelSpawner.h"
#include "VoxelSpawnerGroup.generated.h"

class UVoxelSpawnerGroup;

class VOXEL_API FVoxelSpawnerGroupProxyResult : public FVoxelSpawnerProxyResult
{
public:
	const TArray<TUniquePtr<FVoxelSpawnerProxyResult>> Results;

	FVoxelSpawnerGroupProxyResult(
		const FVoxelSpawnerProxy& Proxy,
		TArray<TUniquePtr<FVoxelSpawnerProxyResult>>&& Results);

	//~ Begin FVoxelSpawnerProxyResult override
	virtual void Apply_AnyThread() override;
	virtual void Apply_GameThread() override;
	//~ End FVoxelSpawnerProxyResult override
};

class VOXEL_API FVoxelSpawnerGroupProxy : public FVoxelSpawnerProxy
{
public:
	const TWeakObjectPtr<UVoxelSpawnerGroup> SpawnerGroup;

	FVoxelSpawnerGroupProxy(UVoxelSpawnerGroup* Spawner, FVoxelSpawnerManager& Manager);

	//~ Begin FVoxelSpawnerProxy Interface
	virtual TUniquePtr<FVoxelSpawnerProxyResult> ProcessHits(
		const FIntBox& Bounds,
		const TArray<FVoxelSpawnerHit>& Hits,
		const FVoxelConstDataAccelerator& Accelerator) const override;
	virtual void PostSpawn() override;
	//~ End FVoxelSpawnerProxy Interface

private:
	struct FChild
	{
		TVoxelSharedPtr<FVoxelSpawnerProxy> Spawner;
		float ProbabilitySum;
	};
	TArray<FChild> Children;

	int32 GetChild(float RandomNumber) const;
};

USTRUCT()
struct FVoxelSpawnerGroupChild
{
	GENERATED_BODY()

		UPROPERTY(EditAnywhere, Category = "Config")
		UVoxelSpawner* Spawner = nullptr;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = 0, ClampMax = 1, UIMin = 0, UIMax = 1))
		float Probability = 0;
};

UCLASS()
class VOXEL_API UVoxelSpawnerGroup : public UVoxelSpawner
{
	GENERATED_BODY()

public:
	// Probabilities do not need to be normalized, although it might be harder to understand what's happening if they're not
	UPROPERTY(EditAnywhere, Category = "Config")
		bool bNormalizeProbabilitiesOnEdit = true;

	UPROPERTY(EditAnywhere, Category = "Config")
		TArray<FVoxelSpawnerGroupChild> Children;

	//~ Begin UVoxelSpawner Interface
	virtual TVoxelSharedRef<FVoxelSpawnerProxy> GetSpawnerProxy(FVoxelSpawnerManager& Manager) override;
	virtual bool GetSpawners(TSet<UVoxelSpawner*>& OutSpawners) override;
	//~ End UVoxelSpawner Interface

protected:
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface
};