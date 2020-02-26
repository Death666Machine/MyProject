// Copyright 2020 Phyronnaz

#include "VoxelSpawners/VoxelSpawner.h"
#include "VoxelSpawners/VoxelSpawnerManager.h"

FVoxelSpawnerProxyResult::FVoxelSpawnerProxyResult(const FVoxelSpawnerProxy& Proxy)
	: Manager(Proxy.Manager)
{
}

///////////////////////////////////////////////////////////////////////////////

FVoxelSpawnerProxy::FVoxelSpawnerProxy(UVoxelSpawner* Spawner, FVoxelSpawnerManager& Manager, uint32 Seed)
	: Manager(Manager)
	, SpawnerSeed(FVoxelUtilities::MurmurHash32(FCrc::StrCrc32(*Spawner->GetPathName())) + Seed)
{
}

TVoxelSharedRef<FVoxelSpawnerProxy> UVoxelSpawner::GetSpawnerProxy(FVoxelSpawnerManager& Manager)
{
	unimplemented();
	return TVoxelSharedPtr<FVoxelSpawnerProxy>().ToSharedRef();
}

bool UVoxelSpawner::GetSpawners(TSet<UVoxelSpawner*>& OutSpawners)
{
	check(this);
	OutSpawners.Add(this);
	return true;
}