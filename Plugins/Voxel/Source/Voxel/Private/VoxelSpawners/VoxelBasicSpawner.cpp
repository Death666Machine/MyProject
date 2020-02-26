// Copyright 2020 Phyronnaz

#include "VoxelSpawners/VoxelBasicSpawner.h"

FVoxelBasicSpawnerProxy::FVoxelBasicSpawnerProxy(UVoxelBasicSpawner* Spawner, FVoxelSpawnerManager& Manager, uint32 Seed)
	: FVoxelSpawnerProxy(Spawner, Manager, Seed)
	, GroundSlopeAngle(Spawner->GroundSlopeAngle)
	, Scaling(Spawner->Scaling)
	, RotationAlignment(Spawner->RotationAlignment)
	, bRandomYaw(Spawner->bRandomYaw)
	, RandomPitchAngle(Spawner->RandomPitchAngle)
	, PositionOffset(Spawner->PositionOffset)
	, RotationOffset(Spawner->RotationOffset)
{
}