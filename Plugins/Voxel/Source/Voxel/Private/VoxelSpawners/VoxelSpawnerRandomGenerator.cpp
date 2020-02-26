// Copyright 2020 Phyronnaz

#include "VoxelSpawners/VoxelSpawnerRandomGenerator.h"
#include "VoxelBaseUtilities.h"
#include "Math/Sobol.h"

FVoxelSpawnerSobolRandomGenerator::FVoxelSpawnerSobolRandomGenerator(int32 CellBits)
	: CellBits(CellBits)
{
}

void FVoxelSpawnerSobolRandomGenerator::Init(int32 SeedX, int32 SeedY)
{
	Value = FSobol::Evaluate(0, CellBits, FIntPoint::ZeroValue, FIntPoint(SeedX, SeedY));
}

void FVoxelSpawnerSobolRandomGenerator::Next() const
{
	Value = FSobol::Next(Index++, CellBits, Value);
}

template<uint32 Base>
inline float Halton(uint32 Index)
{
	float Result = 0.0f;
	const float InvBase = 1.0f / Base;
	float Fraction = InvBase;
	while (Index > 0)
	{
		Result += (Index % Base) * Fraction;
		Index /= Base;
		Fraction *= InvBase;
	}
	return Result;
}

void FVoxelSpawnerHaltonRandomGenerator::Init(int32 SeedX, int32 SeedY)
{
	// No symmetry!
	Index = FVoxelUtilities::MurmurHash32(SeedX) + FVoxelUtilities::MurmurHash32(SeedY * 23);
	Next();
}

void FVoxelSpawnerHaltonRandomGenerator::Next() const
{
	Value.X = Halton<2>(Index);
	Value.Y = Halton<3>(Index);
	Index++;
}