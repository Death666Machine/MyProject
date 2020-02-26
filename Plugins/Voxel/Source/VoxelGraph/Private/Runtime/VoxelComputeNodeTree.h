// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelGlobals.h"

struct FVoxelNodeType;
class FVoxelCppConstructor;
struct FVoxelWorldGeneratorInit;
struct FVoxelContext;
struct FVoxelContextRange;
class FVoxelComputeNode;
class FVoxelGraphFunction;
class FVoxelDataComputeNode;
class FVoxelSeedComputeNode;
class FVoxelExecComputeNode;
class UVoxelNode;
struct FVoxelGraphVMInitBuffers;
struct FVoxelGraphVMComputeBuffers;
struct FVoxelGraphVMComputeRangeBuffers;
struct FVoxelVariableAccessInfo;

/**
 * Tree of the compute nodes. Used to interpret the graph at runtime and to compile it
 */
struct FVoxelComputeNodeTree
{
	FVoxelComputeNodeTree() = default;

	// Runtime
	void Init(const FVoxelWorldGeneratorInit& InitStruct, FVoxelGraphVMInitBuffers& Buffers) const;
	template<bool bEnableStats>
	const FVoxelGraphFunction* Compute(const FVoxelContext& Context, FVoxelGraphVMComputeBuffers& Buffers) const;
	template<bool bEnableRangeAnalysisDebug>
	const FVoxelGraphFunction* ComputeRange(const FVoxelContextRange& Context, FVoxelGraphVMComputeRangeBuffers& Buffers) const;

	// Compilation
	void GetNodes(TSet<FVoxelComputeNode*>& Nodes) const;

	void InitCpp(FVoxelCppConstructor& Constructor) const;
	void ComputeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, const TArray<FString>& GraphOutputs);
	void ComputeRangeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, const TArray<FString>& GraphOutputs);

	inline const TArray<FVoxelDataComputeNode*>& GetDataNodes() const { return DataNodes; }
	inline const TArray<FVoxelSeedComputeNode*>& GetSeedNodes() const { return SeedNodes; }
	inline const FVoxelExecComputeNode* GetExecNode() const { return ExecNode; }
	inline const TArray<FVoxelComputeNodeTree>& GetChildren() const { return Children; }

private:
	TArray<FVoxelDataComputeNode*> DataNodes;
	TArray<TVoxelSharedRef<FVoxelDataComputeNode>> DataNodesRefs;

	TArray<FVoxelSeedComputeNode*> SeedNodes;
	TArray<TVoxelSharedRef<FVoxelSeedComputeNode>> SeedNodesRefs;

	FVoxelExecComputeNode* ExecNode = nullptr;
	TVoxelSharedPtr<FVoxelExecComputeNode> ExecNodeRef;

	TArray<FVoxelComputeNodeTree> Children;

	friend class FVoxelCompilationNodeTree;
};
