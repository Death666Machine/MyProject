// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelGlobals.h"
#include "VoxelGraphGlobals.h"
#include "Runtime/VoxelNodeType.h"

struct FVoxelGraphVMInitBuffers
{
	Seed* RESTRICT const Variables;

	FVoxelGraphVMInitBuffers(Seed* RESTRICT Variables)
		: Variables(Variables)
	{
	}
};

struct FVoxelGraphVMComputeBuffers
{
	FVoxelNodeType* RESTRICT const Variables;
	FVoxelNodeType FunctionInputsOutputs[MAX_VOXELFUNCTION_ARGS];
	FVoxelNodeType GraphOutputs[MAX_VOXELGRAPH_OUTPUTS];

	FVoxelGraphVMComputeBuffers(FVoxelNodeType* RESTRICT Variables)
		: Variables(Variables)
	{
	}
};

struct FVoxelGraphVMComputeRangeBuffers
{
	FVoxelNodeRangeType* RESTRICT const Variables;
	FVoxelNodeRangeType FunctionInputsOutputs[MAX_VOXELFUNCTION_ARGS];
	FVoxelNodeRangeType GraphOutputs[MAX_VOXELGRAPH_OUTPUTS];

	FVoxelGraphVMComputeRangeBuffers(FVoxelNodeRangeType* RESTRICT Variables)
		: Variables(Variables)
	{
	}
};
