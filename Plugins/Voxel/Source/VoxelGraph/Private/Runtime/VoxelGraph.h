// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelGlobals.h"
#include "VoxelAxisDependencies.h"

class FVoxelGraphFunction;
struct FVoxelNodeType;
struct FVoxelWorldGeneratorInit;
struct FVoxelContext;
struct FVoxelContextRange;
class FVoxelCppConstructor;
struct FVoxelGraphVMInitBuffers;
struct FVoxelGraphVMComputeBuffers;
struct FVoxelGraphVMComputeRangeBuffers;
class FVoxelDataComputeNode;
class FVoxelSeedComputeNode;
class FVoxelComputeNode;
class FVoxelVariable;

struct FVoxelGraphFunctions
{
	int32 FunctionId = -1;
	TVoxelSharedPtr<FVoxelGraphFunction> FunctionX;
	TVoxelSharedPtr<FVoxelGraphFunction> FunctionXYWithCache;
	TVoxelSharedPtr<FVoxelGraphFunction> FunctionXYWithoutCache;
	TVoxelSharedPtr<FVoxelGraphFunction> FunctionXYZWithCache;
	TVoxelSharedPtr<FVoxelGraphFunction> FunctionXYZWithoutCache;

	FVoxelGraphFunctions() = default;
	FVoxelGraphFunctions(
		int32 FunctionId,
		const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionX,
		const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionXYWithCache,
		const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionXYWithoutCache,
		const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionXYZWithCache,
		const TVoxelSharedPtr<FVoxelGraphFunction>& FunctionXYZWithoutCache);

	inline const FVoxelGraphFunction& Get(EVoxelFunctionAxisDependencies Dependencies) const
	{
		switch (Dependencies)
		{
		case EVoxelFunctionAxisDependencies::X:
			return *FunctionX.Get();
		case EVoxelFunctionAxisDependencies::XYWithCache:
			return *FunctionXYWithCache.Get();
		case EVoxelFunctionAxisDependencies::XYWithoutCache:
			return *FunctionXYWithoutCache.Get();
		case EVoxelFunctionAxisDependencies::XYZWithCache:
			return *FunctionXYZWithCache.Get();
		case EVoxelFunctionAxisDependencies::XYZWithoutCache:
			return *FunctionXYZWithoutCache.Get();
		default:
			check(false);
			return *FunctionX.Get();
		}
	}
	inline bool IsValid() const
	{
		return FunctionX.IsValid();
	}
	inline TArray<FVoxelGraphFunction*, TFixedAllocator<5>> Iterate() const
	{
		return
		{
			FunctionX.Get(),
			FunctionXYWithCache.Get(),
			FunctionXYWithoutCache.Get(),
			FunctionXYZWithCache.Get(),
			FunctionXYZWithoutCache.Get(),
		};
	}
};

class FVoxelGraph : public TVoxelSharedFromThis<FVoxelGraph>
{
public:
	FVoxelGraph(
		const FString& Name,
		const TArray<FVoxelGraphFunctions>& Functions,
		const FVoxelGraphFunctions& FirstFunctions,
		const TArray<TVoxelSharedRef<FVoxelDataComputeNode>>& ConstantComputeNodes,
		const TArray<TVoxelSharedRef<FVoxelSeedComputeNode>>& SeedComputeNodes,
		int32 VariablesBufferSize,
		bool bEnableStats,
		bool bEnableRangeAnalysisDebug);

	const FString Name;
	const TArray<FVoxelGraphFunctions> AllFunctions;
	const FVoxelGraphFunctions FirstFunctions;
	const TArray<TVoxelSharedRef<FVoxelDataComputeNode>> ConstantComputeNodes;
	const TArray<TVoxelSharedRef<FVoxelSeedComputeNode>> SeedComputeNodes;
	const int32 VariablesBufferSize = 0;
	const bool bEnableStats;
	const bool bEnableRangeAnalysisDebug;

public:
	void Init(const FVoxelWorldGeneratorInit& InitStruct, FVoxelGraphVMInitBuffers& Buffers) const;
	void ComputeConstants(FVoxelGraphVMComputeBuffers& Buffers) const;
	void Compute(const FVoxelContext& Context, FVoxelGraphVMComputeBuffers& Buffers, EVoxelFunctionAxisDependencies Dependencies) const;

	void ComputeRangeConstants(FVoxelGraphVMComputeRangeBuffers& Buffers) const;
	void ComputeRange(const FVoxelContextRange& Context, FVoxelGraphVMComputeRangeBuffers& Buffers) const;

public:
	void GetConstantNodes(TSet<FVoxelComputeNode*>& Nodes) const;
	void GetNotConstantNodes(TSet<FVoxelComputeNode*>& Nodes) const;
	void GetAllNodes(TSet<FVoxelComputeNode*>& Nodes) const;

	void Init(FVoxelCppConstructor& Constructor) const;
	void ComputeConstants(FVoxelCppConstructor& Constructor) const;
	void Compute(FVoxelCppConstructor& Constructor, EVoxelFunctionAxisDependencies Dependencies) const;

	void DeclareInitFunctions(FVoxelCppConstructor& Constructor) const;
	void DeclareComputeFunctions(FVoxelCppConstructor& Constructor, const TArray<FString>& GraphOutputs) const;
};
