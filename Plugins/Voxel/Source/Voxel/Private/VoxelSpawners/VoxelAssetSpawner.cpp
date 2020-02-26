// Copyright 2020 Phyronnaz

#include "VoxelSpawners/VoxelAssetSpawner.h"
#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelData/VoxelData.h"
#include "VoxelRender/IVoxelLODManager.h"
#include "VoxelWorldGenerators/VoxelEmptyWorldGenerator.h"
#include "VoxelPlaceableItems/VoxelAssetItem.h"
#include "VoxelMessages.h"

#include "Async/Async.h"

FVoxelAssetSpawnerProxyResult::FVoxelAssetSpawnerProxyResult(
	const FVoxelAssetSpawnerProxy& Proxy,
	TArray<FMatrix>&& Transforms,
	TArray<int32>&& GeneratorsIndices)
	: FVoxelSpawnerProxyResult(Proxy)
	, Proxy(Proxy)
	, Transforms(MoveTemp(Transforms))
	, GeneratorsIndices(MoveTemp(GeneratorsIndices))
{
	check(Transforms.Num() == GeneratorsIndices.Num());
}

void FVoxelAssetSpawnerProxyResult::Apply_AnyThread()
{
	if (Transforms.Num() == 0) return;

	FIntBoxWithValidity BoundsToLock;
	TransientGeneratorsBounds.Reset(Transforms.Num());

	for (const auto& Matrix : Transforms)
	{
		const FIntBox GeneratorBounds = Proxy.GeneratorLocalBounds.ApplyTransform(FTransform(Matrix));
		BoundsToLock += GeneratorBounds;
		TransientGeneratorsBounds.Emplace(GeneratorBounds);
	}

	auto& Data = *Manager.Settings.Data;

	FVoxelWriteScopeLock Lock(Data, BoundsToLock.GetBox(), FUNCTION_FNAME);
	for (int32 Index = 0; Index < Transforms.Num(); Index++)
	{
		const auto& Generator = Proxy.Generators[GeneratorsIndices[Index]].ToSharedRef();
		Data.AddItem<FVoxelAssetItem>(Generator, TransientGeneratorsBounds[Index], FTransform(Transforms[Index]), Proxy.Priority);
	}
}

void FVoxelAssetSpawnerProxyResult::Apply_GameThread()
{
	Manager.Settings.LODManager->UpdateBounds(TransientGeneratorsBounds);
	TransientGeneratorsBounds.Empty();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TArray<TVoxelSharedPtr<FVoxelTransformableWorldGeneratorInstance>> CreateGenerators(UVoxelAssetSpawner& Spawner, FVoxelSpawnerManager& Manager)
{
	TArray<TVoxelSharedPtr<FVoxelTransformableWorldGeneratorInstance>> Result;
	if (!Spawner.Generator.IsValid())
	{
		Result.Add(MakeShareable(new FVoxelTransformableEmptyWorldGeneratorInstance()));
		return Result;
	}

	const FRandomStream Stream(FCrc::StrCrc32(*Spawner.GetPathName()));
	for (int32 Index = 0; Index < FMath::Max(1, Spawner.NumberOfDifferentSeedsToUse); Index++)
	{
		auto NewGenerator = Spawner.Generator.GetInstance(false);

		FVoxelWorldGeneratorInit Init;
		Init.VoxelSize = Manager.Settings.VoxelSize;
		Init.Seeds = Manager.Settings.Seeds;

		for (FName Seed : Spawner.Seeds)
		{
			Init.Seeds.Add(Seed, int64(double(Stream.GetFraction() * 2 - 1) * MAX_int32));
		}

		NewGenerator->Init(Init);

		Result.Add(NewGenerator);
	}

	return Result;
}

FVoxelAssetSpawnerProxy::FVoxelAssetSpawnerProxy(UVoxelAssetSpawner* Spawner, FVoxelSpawnerManager& Manager)
	: FVoxelBasicSpawnerProxy(Spawner, Manager)
	, Generators(CreateGenerators(*Spawner, Manager))
	, GeneratorLocalBounds(Spawner->GeneratorLocalBounds)
	, Priority(Spawner->Priority)
	, bRoundAssetPosition(Spawner->bRoundAssetPosition)
{
}

FVoxelAssetSpawnerProxy::~FVoxelAssetSpawnerProxy()
{
}

TUniquePtr<FVoxelSpawnerProxyResult> FVoxelAssetSpawnerProxy::ProcessHits(
	const FIntBox& Bounds,
	const TArray<FVoxelSpawnerHit>& Hits,
	const FVoxelConstDataAccelerator& Accelerator) const
{
	const uint32 Seed = Bounds.GetMurmurHash() ^ SpawnerSeed;
	const auto& Settings = Manager.Settings;
	auto& Data = *Settings.Data;
	const auto& WorldGenerator = *Data.WorldGenerator;

	const FRandomStream Stream(Seed);

	TArray<FMatrix> Transforms;
	Transforms.Reserve(Hits.Num());

	for (auto& Hit : Hits)
	{
		const auto& Position = Hit.Position;
		const auto& Normal = Hit.Normal;
		const auto WorldUp = WorldGenerator.GetUpVector(Position);

		if (!CanSpawn(Normal, WorldUp))
		{
			continue;
		}

		const FMatrix Matrix = GetMatrixWithoutOffsets(Stream, Normal, WorldUp);

		const FVector RotatedPositionOffset = Matrix.TransformVector(PositionOffset);

		FMatrix Transform = Matrix;
		Transform *= RotationOffset;
		const auto P = bRoundAssetPosition ? FVector(FVoxelUtilities::RoundToInt(Position)) : Position;
		Transform = Transform.ConcatTranslation(RotatedPositionOffset + P);

		Transforms.Emplace(Transform);
	}

	if (Transforms.Num() == 0)
	{
		return nullptr;
	}
	else
	{
		Transforms.Shrink();

		TArray<int32> GeneratorsIndices;
		GeneratorsIndices.Reserve(Transforms.Num());
		for (auto& Transform : Transforms)
		{
			GeneratorsIndices.Add(Stream.RandHelper(Generators.Num()));
		}

		return MakeUnique<FVoxelAssetSpawnerProxyResult>(*this, MoveTemp(Transforms), MoveTemp(GeneratorsIndices));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelSharedRef<FVoxelSpawnerProxy> UVoxelAssetSpawner::GetSpawnerProxy(FVoxelSpawnerManager& Manager)
{
	if (!Generator.IsValid())
	{
		FVoxelMessages::Error("Invalid generator!", this);
	}
	return MakeVoxelShared<FVoxelAssetSpawnerProxy>(this, Manager);
}
#undef LOCTEXT_NAMESPACE