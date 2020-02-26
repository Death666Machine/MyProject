// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelGlobals.h"
#include "VoxelTexture.h"

template<class T>
struct TArray3
{
	FIntVector Size = FIntVector(ForceInit);
	TArray<T> Data;

	TArray3() = default;
	explicit TArray3(const FIntVector& Size)
		: Size(Size)
	{
		Data.SetNumUninitialized(Size.X * Size.Y * Size.Z);
	}

	FORCEINLINE const T& operator()(const FIntVector& P) const
	{
		return (*this)(P.X, P.Y, P.Z);
	}
	FORCEINLINE T& operator()(const FIntVector& P)
	{
		return (*this)(P.X, P.Y, P.Z);
	}

	FORCEINLINE const T& operator()(int32 I, int32 J, int32 K) const
	{
		return const_cast<TArray3<T>&>(*this)(I, J, K);
	}
	FORCEINLINE T& operator()(int32 I, int32 J, int32 K)
	{
		checkVoxelSlow(0 <= I && I < Size.X);
		checkVoxelSlow(0 <= J && J < Size.Y);
		checkVoxelSlow(0 <= K && K < Size.Z);
		return static_cast<T* RESTRICT>(Data.GetData())[I + J * Size.X + K * Size.X * Size.Y];
	}

	void Resize(const FIntVector& NewSize)
	{
		VOXEL_FUNCTION_COUNTER();
		Size = NewSize;
		Data.SetNumUninitialized(Size.X * Size.Y * Size.Z);
	}

	void Assign(const T& Value)
	{
		VOXEL_FUNCTION_COUNTER();
		for (auto& X : Data)
		{
			X = Value;
		}
	}

	void Memzero()
	{
		VOXEL_FUNCTION_COUNTER();
		FMemory::Memzero(Data.GetData(), Data.Num() * sizeof(T));
	}
};

template<typename T>
struct TFastArrayView
{
	TFastArrayView() = default;

	TFastArrayView(const TArray<T>& Other)
		: DataPtr(Other.GetData())
		, ArrayNum(Other.Num())
	{
	}

	inline bool IsValidIndex(int32 Index) const
	{
		return (Index >= 0) && (Index < ArrayNum);
	}
	inline int32 Num() const
	{
		return ArrayNum;
	}
	FORCEINLINE const T& operator[](int32 Index) const
	{
		checkVoxelSlow(IsValidIndex(Index));
		return DataPtr[Index];
	}

private:
	const T* RESTRICT DataPtr = nullptr;
	int32 ArrayNum = 0;
};

struct FMakeLevelSet3Settings
{
	TFastArrayView<FVector> Vertices;
	TFastArrayView<FVector2D> UVs;
	TFastArrayView<FIntVector> Triangles;

	bool bDoSweep;
	bool bComputeSign;
	bool bHideLeaks;

	FVector Origin;
	float Delta;
	FIntVector Size;

	int32 ExactBand;

	bool bExportUVs;
	TArray<TVoxelTexture<FColor>> ColorTextures;
};

// First the distances are set around triangles in a radius of ExactBand voxels
// Then we optionally propagate those distances by sweeping (takes a while)
// Finally, we set the signs by sweeping along the X axis
VOXEL_API void MakeLevelSet3(
	const FMakeLevelSet3Settings& Settings,
	TArray3<float>& OutPhi,
	TArray3<FVector2D>& OutUVs,
	TArray<TArray3<FColor>>& OutColors,
	int32& OutNumLeaks);
