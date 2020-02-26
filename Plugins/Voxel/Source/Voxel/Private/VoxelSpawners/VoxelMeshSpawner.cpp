// Copyright 2020 Phyronnaz

#include "VoxelSpawners/VoxelMeshSpawner.h"
#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelSpawners/VoxelInstancedMeshManager.h"
#include "VoxelSpawners/VoxelSpawnerGroup.h"
#include "VoxelData/VoxelData.h"
#include "VoxelData/VoxelDataAccelerator.h"
#include "VoxelMessages.h"

#include "Async/Async.h"
#include "Engine/StaticMesh.h"

FVoxelMeshSpawnerProxyResult::FVoxelMeshSpawnerProxyResult(
	const FVoxelMeshSpawnerProxy& Proxy,
	const FIntBox& Bounds,
	TArray<FVoxelSpawnerMatrix>&& Matrices)
	: FVoxelSpawnerProxyResult(Proxy)
	, Proxy(Proxy)
	, Bounds(Bounds)
	, Matrices(MoveTemp(Matrices))
{
}

void FVoxelMeshSpawnerProxyResult::Apply_GameThread()
{
	check(IsInGameThread());

	auto& MeshManager = *Manager.Settings.MeshManager;
	if (Proxy.bAlwaysSpawnActor)
	{
		MeshManager.SpawnActors(Proxy.InstanceSettings.ActorTemplate, Proxy.InstanceSettings.Mesh, Matrices);
	}
	else
	{
		MeshManager.AddInstances(Proxy.InstanceSettings, Matrices, Bounds);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

inline FVoxelInstancedMeshSettings WithMeshAndActor(
	FVoxelInstancedMeshSettings Settings,
	TWeakObjectPtr<UStaticMesh> InMesh,
	TSubclassOf<AVoxelSpawnerActor> InActorTemplate)
{
	Settings.Mesh = InMesh;
	Settings.ActorTemplate = InActorTemplate;
	return Settings;
}

FVoxelMeshSpawnerProxy::FVoxelMeshSpawnerProxy(UVoxelMeshSpawnerBase* Spawner, TWeakObjectPtr<UStaticMesh> Mesh, FVoxelSpawnerManager& Manager, uint32 Seed)
	: FVoxelBasicSpawnerProxy(Spawner, Manager, Seed)
	, InstanceSettings(WithMeshAndActor(Spawner->InstancedMeshSettings, Mesh, Spawner->ActorTemplate))
	, bAlwaysSpawnActor(Spawner->bAlwaysSpawnActor)
	, bSendVoxelMaterialThroughInstanceRandom(Spawner->bSendVoxelMaterialThroughInstanceRandom)
	, FloatingDetectionOffset(Spawner->FloatingDetectionOffset)
{
}

TUniquePtr<FVoxelSpawnerProxyResult> FVoxelMeshSpawnerProxy::ProcessHits(
	const FIntBox& Bounds,
	const TArray<FVoxelSpawnerHit>& Hits,
	const FVoxelConstDataAccelerator& Accelerator) const
{
	check(Hits.Num() > 0);

	const uint32 Seed = Bounds.GetMurmurHash() ^ SpawnerSeed;
	auto& WorldGenerator = *Manager.Settings.Data->WorldGenerator;
	const float VoxelSize = Manager.Settings.VoxelSize;

	const FRandomStream Stream(Seed);

	TArray<FVoxelSpawnerMatrix> Matrices;
	Matrices.Reserve(Hits.Num());

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
		Transform = Transform.ConcatTranslation(RotatedPositionOffset + VoxelSize * Position);
		FVoxelSpawnerMatrix SpawnerMatrix(Transform);

		if (bSendVoxelMaterialThroughInstanceRandom)
		{
			// Note: instead of RoundToInt, should maybe use the nearest voxel that's not empty?
			const auto Material = Accelerator.GetMaterial(FVoxelUtilities::RoundToInt(Position), 0);
			SpawnerMatrix.SetRandomInstanceId(Material.GetPackedInt());
		}
		else
		{
			SpawnerMatrix.SetRandomInstanceId(Stream.GetFraction());
		}

		const FVector RotatedFloatingDetectionOffset = Matrix.TransformVector(FloatingDetectionOffset);
		SpawnerMatrix.SetPositionOffset(RotatedPositionOffset - RotatedFloatingDetectionOffset);

		Matrices.Add(SpawnerMatrix);
	}

	if (Matrices.Num() == 0)
	{
		return nullptr;
	}
	else
	{
		Matrices.Shrink();
		return MakeUnique<FVoxelMeshSpawnerProxyResult>(*this, Bounds, MoveTemp(Matrices));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

inline TArray<TVoxelSharedPtr<FVoxelMeshSpawnerProxy>> CreateMeshProxies(UVoxelMeshSpawnerGroup* Spawner, FVoxelSpawnerManager& Manager)
{
	TArray<TVoxelSharedPtr<FVoxelMeshSpawnerProxy>> Result;
	uint32 Seed = 1;
	for (auto* Mesh : Spawner->Meshes)
	{
		Result.Add(MakeVoxelShared<FVoxelMeshSpawnerProxy>(Spawner, Mesh, Manager, Seed++));
	}
	return Result;
}

FVoxelMeshSpawnerGroupProxy::FVoxelMeshSpawnerGroupProxy(UVoxelMeshSpawnerGroup* Spawner, FVoxelSpawnerManager& Manager)
	: FVoxelSpawnerProxy(Spawner, Manager)
	, Proxies(CreateMeshProxies(Spawner, Manager))
{
}

TUniquePtr<FVoxelSpawnerProxyResult> FVoxelMeshSpawnerGroupProxy::ProcessHits(
	const FIntBox& Bounds,
	const TArray<FVoxelSpawnerHit>& Hits,
	const FVoxelConstDataAccelerator& Accelerator) const
{
	TArray<TUniquePtr<FVoxelSpawnerProxyResult>> Results;

	const int32 NumHitsPerProxy = FVoxelUtilities::DivideCeil(Hits.Num(), Proxies.Num());
	int32 HitsStartIndex = 0;
	for (auto& Proxy : Proxies)
	{
		const int32 HitsEndIndex = FMath::Min(HitsStartIndex + NumHitsPerProxy, Hits.Num());
		if (HitsStartIndex < HitsEndIndex)
		{
			TArray<FVoxelSpawnerHit> ProxyHits(Hits.GetData() + HitsStartIndex, HitsEndIndex - HitsStartIndex);
			auto Result = Proxy->ProcessHits(Bounds, ProxyHits, Accelerator);
			if (Result.IsValid())
			{
				Results.Emplace(MoveTemp(Result));
			}
		}
		HitsStartIndex = HitsEndIndex;
	}

	if (Results.Num() == 0)
	{
		return nullptr;
	}
	else
	{
		return MakeUnique<FVoxelSpawnerGroupProxyResult>(*this, MoveTemp(Results));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void UVoxelMeshSpawnerBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (!IsTemplate())
	{
		InstancedMeshSettings.BodyInstance.FixupData(this);
	}
}

///////////////////////////////////////////////////////////////////////////////

TVoxelSharedRef<FVoxelSpawnerProxy> UVoxelMeshSpawner::GetSpawnerProxy(FVoxelSpawnerManager& Manager)
{
	if (!Mesh && !bAlwaysSpawnActor)
	{
		FVoxelMessages::Error("Invalid mesh!", this);
	}
	return MakeVoxelShared<FVoxelMeshSpawnerProxy>(this, Mesh, Manager);
}

///////////////////////////////////////////////////////////////////////////////

TVoxelSharedRef<FVoxelSpawnerProxy> UVoxelMeshSpawnerGroup::GetSpawnerProxy(FVoxelSpawnerManager& Manager)
{
	if (!bAlwaysSpawnActor)
	{
		for (auto* Mesh : Meshes)
		{
			if (!Mesh)
			{
				FVoxelMessages::Error("Invalid mesh!", this);
			}
		}
	}
	return MakeVoxelShared<FVoxelMeshSpawnerGroupProxy>(this, Manager);
}

float UVoxelMeshSpawnerGroup::GetDistanceBetweenInstancesInVoxel() const
{
	// Scale it to account for instances split between meshes
	return DistanceBetweenInstancesInVoxel / FMath::Max(1, Meshes.Num());
}
#undef LOCTEXT_NAMESPACE