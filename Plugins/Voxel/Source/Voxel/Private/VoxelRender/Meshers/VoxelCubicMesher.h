// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "StackArray.h"
#include "VoxelDirection.h"
#include "VoxelData/VoxelDataAccelerator.h"
#include "VoxelRender/Meshers/VoxelMesher.h"

#define CUBIC_CHUNK_SIZE_WITH_NEIGHBORS (RENDER_CHUNK_SIZE + 2)

class FVoxelCubicMesher : public FVoxelMesher
{
public:
	using FVoxelMesher::FVoxelMesher;

protected:
	virtual FIntBox GetBoundsToCheckIsEmptyOn() const override final;
	virtual FIntBox GetBoundsToLock() const override final;
	virtual TVoxelSharedPtr<FVoxelChunkMesh> CreateFullChunkImpl(FVoxelMesherTimes& Times) override final;
	virtual void CreateGeometryImpl(FVoxelMesherTimes& Times, TArray<uint32>& Indices, TArray<FVector>& Vertices) override final;

private:
	TUniquePtr<FVoxelConstDataAccelerator> Accelerator;
	TStackArray<FVoxelValue, CUBIC_CHUNK_SIZE_WITH_NEIGHBORS * CUBIC_CHUNK_SIZE_WITH_NEIGHBORS * CUBIC_CHUNK_SIZE_WITH_NEIGHBORS> CachedValues;

private:
	template<typename T>
	void CreateGeometryTemplate(FVoxelMesherTimes& Times, TArray<uint32>& Indices, TArray<T>& Vertices);

private:
	FVoxelValue GetValue(int32 X, int32 Y, int32 Z) const;
};

class FVoxelCubicTransitionsMesher : public FVoxelTransitionsMesher
{
public:
	using FVoxelTransitionsMesher::FVoxelTransitionsMesher;

protected:
	virtual FIntBox GetBoundsToCheckIsEmptyOn() const override final;
	virtual FIntBox GetBoundsToLock() const override final;
	virtual TVoxelSharedPtr<FVoxelChunkMesh> CreateFullChunkImpl(FVoxelMesherTimes& Times) override final;

private:
	TUniquePtr<FVoxelConstDataAccelerator> Accelerator;

private:
	template<EVoxelDirection::Type Direction, typename TVertex>
	void CreateTransitionsForDirection(FVoxelMesherTimes& Times, TArray<uint32>& Indices, TArray<TVertex>& Vertices);

private:
	template<EVoxelDirection::Type Direction>
	FVoxelValue GetValue(int32 InStep, int32 X, int32 Y, int32 Z) const;
	template<EVoxelDirection::Type Direction>
	FVoxelMaterial GetMaterial(int32 InStep, int32 X, int32 Y, int32 Z) const;

	// LX * HalfStep = GX
	template<EVoxelDirection::Type Direction, EVoxelDirection::Type FaceDirection, typename TVertex>
	void Add2DFace(
		int32 InStep,
		const FVoxelMaterial& Material,
		int32 LX, int32 LY,
		TArray<TVertex>& Vertices, TArray<uint32>& Indices);

	template<EVoxelDirection::Type Direction>
	static FIntVector Local2DToGlobal(int32 InSize, int32 LX, int32 LY, int32 LZ);
};
