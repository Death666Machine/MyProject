// Copyright 2020 Phyronnaz

#include "VoxelPlaceableItems/VoxelDefaultItems.h"

FString FVoxelPerlinWorm::GetDescription() const
{
	return FString::Printf(TEXT("Perlin worm from %s to %s, radius %f"), *Start.ToString(), *GetEnd().ToString(), Radius);
}

FString FVoxelDisableEditsBoxItem::GetDescription() const
{
	return FString::Printf(TEXT("Disable edits box on %s"), *Bounds.ToString());
}