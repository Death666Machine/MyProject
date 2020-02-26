// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelAxisDependencies.h"
#include "VoxelGlobals.h"

class FVoxelComputeNode;
class FVoxelCppConstructor;

struct FVoxelComputeNodeTree;
struct FVoxelWorldGeneratorInit;

struct FVoxelGraphVMInitBuffers;
struct FVoxelGraphVMComputeBuffers;
struct FVoxelGraphVMComputeRangeBuffers;

struct FVoxelContext;
struct FVoxelContextRange;

enum class EVoxelFunctionType : uint8
{
	Init,
	Compute
};

struct FVoxelGraphFunctionInfo
{
	const int32 FunctionId;
	const EVoxelFunctionType FunctionType;
	const EVoxelFunctionAxisDependencies Dependencies;

	FVoxelGraphFunctionInfo(int32 FunctionId, EVoxelFunctionType FunctionType, EVoxelFunctionAxisDependencies Dependencies)
		: FunctionId(FunctionId)
		, FunctionType(FunctionType)
		, Dependencies(Dependencies)
	{
	}

	FString GetFunctionName() const;
};

class FVoxelGraphFunction
{
public:
	const int32 FunctionId;
	const EVoxelFunctionAxisDependencies Dependencies;

	FVoxelGraphFunction(
		const TVoxelSharedRef<FVoxelComputeNodeTree>& Tree,
		const TVoxelSharedRef<FVoxelComputeNode>& FunctionInit,
		int32 FunctionId,
		EVoxelFunctionAxisDependencies Dependencies);

	void Call(FVoxelCppConstructor& Constructor, const TArray<FString>& Args, EVoxelFunctionType FunctionType) const;

	bool IsUsedForInit() const;
	bool IsUsedForCompute(FVoxelCppConstructor& Constructor) const;

	inline const FVoxelComputeNodeTree& GetTree() const
	{
		return *Tree;
	}

public:
	void Init(const FVoxelWorldGeneratorInit& InitStruct, FVoxelGraphVMInitBuffers& Buffers) const;
	template<bool bEnableStats>
	void Compute(const FVoxelContext& Context, FVoxelGraphVMComputeBuffers& Buffers) const;
	template<bool bEnableRangeAnalysisDebug>
	void ComputeRange(const FVoxelContextRange& Context, FVoxelGraphVMComputeRangeBuffers& Buffers) const;

public:
	void GetNodes(TSet<FVoxelComputeNode*>& Nodes) const;

	void DeclareInitFunction(FVoxelCppConstructor& Constructor) const;
	void DeclareComputeFunction(FVoxelCppConstructor& Constructor, const TArray<FString>& GraphOutputs) const;

private:
	const TVoxelSharedRef<FVoxelComputeNodeTree> Tree;
	const TVoxelSharedRef<FVoxelComputeNode> FunctionInit;

	void DeclareFunction(FVoxelCppConstructor& Constructor, EVoxelFunctionType Type) const;
};
