// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"

// Matrix with special meaning for the last column
// Matrix[0][3] : random instance id, // Matrix[1 2 3][3]: position offset (used for voxel lookup)
struct FVoxelSpawnerMatrix
{
	FVoxelSpawnerMatrix() = default;
	explicit FVoxelSpawnerMatrix(const FMatrix & Matrix)
		: Matrix(Matrix)
	{
	}

	FORCEINLINE float GetRandomInstanceId() const
	{
		return Matrix.M[0][3];
	}
	FORCEINLINE void SetRandomInstanceId(float RandomInstanceId)
	{
		Matrix.M[0][3] = RandomInstanceId;
	}
	FORCEINLINE void SetRandomInstanceId(uint32 PackedInt)
	{
		SetRandomInstanceId(*reinterpret_cast<const float*>(&PackedInt));
	}

	// Used for floating detection
	FORCEINLINE FVector GetPositionOffset() const
	{
		return FVector(Matrix.M[1][3], Matrix.M[2][3], Matrix.M[3][3]);
	}
	FORCEINLINE void SetPositionOffset(const FVector& PositionOffset)
	{
		Matrix.M[1][3] = PositionOffset.X;
		Matrix.M[2][3] = PositionOffset.Y;
		Matrix.M[3][3] = PositionOffset.Z;
	}

	FORCEINLINE FMatrix GetCleanMatrix() const
	{
		auto Copy = Matrix;
		Copy.M[0][3] = 0;
		Copy.M[1][3] = 0;
		Copy.M[2][3] = 0;
		Copy.M[3][3] = 1;
		return Copy;
	}

	FORCEINLINE bool operator==(const FVoxelSpawnerMatrix& Other) const
	{
		return Matrix == Other.Matrix;
	}
	FORCEINLINE bool operator!=(const FVoxelSpawnerMatrix& Other) const
	{
		return Matrix != Other.Matrix;
	}

private:
	FMatrix Matrix;
};
