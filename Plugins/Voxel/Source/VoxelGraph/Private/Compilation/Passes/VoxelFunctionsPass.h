// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelCompilationPass.h"

class FVoxelCompilationNode;
struct FVoxelCompilationFunctionDescriptor;

struct FVoxelFindFunctionsPass
{
	VOXEL_PASS_BODY(FVoxelFindFunctionsPass);

	static void Apply(FVoxelGraphCompiler& Compiler, TArray<FVoxelCompilationFunctionDescriptor>& OutFunctions);
};

struct FVoxelRemoveNodesOutsideFunction
{
	VOXEL_PASS_BODY(FVoxelRemoveNodesOutsideFunction);

	static void Apply(FVoxelGraphCompiler& Compiler, TSet<FVoxelCompilationNode*>& FunctionNodes);
};

struct FVoxelAddFirstFunctionPass
{
	VOXEL_PASS_BODY(FVoxelAddFirstFunctionPass);

	static void Apply(FVoxelGraphCompiler& Compiler);
};

struct FVoxelReplaceFunctionSeparatorsPass
{
	VOXEL_PASS_BODY(FVoxelReplaceFunctionSeparatorsPass);

	static void Apply(FVoxelGraphCompiler& Compiler);
};
