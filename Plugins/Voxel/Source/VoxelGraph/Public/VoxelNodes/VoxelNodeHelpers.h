// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelGraphGlobals.h"
#include "VoxelPinCategory.h"
#include "VoxelContext.h"
#include "Runtime/VoxelNodeType.h"
#include "Runtime/VoxelGraphPerfCounter.h"

namespace FVoxelNodeHelpers
{
	template<typename T>
	inline uint64 ComputeStatsImplEx(const T* This, int32 NumberOfLoops)
	{
		FVoxelNodeType Inputs[MAX_VOXELNODE_PINS];
		FVoxelNodeType Outputs[MAX_VOXELNODE_PINS];
		for (auto& Input : Inputs)
		{
			Input.Get<v_flt>() = 1;
		}
		for (auto& Output : Outputs)
		{
			Output.Get<v_flt>() = 1;
		}
		const FVoxelContext Context = FVoxelContext::EmptyContext;

		const uint64 Start = FPlatformTime::Cycles64();
		for (int32 Index = 0; Index < NumberOfLoops; Index++)
		{
			This->T::Compute(Inputs, Outputs, Context);
		}
		const uint64 End = FPlatformTime::Cycles64();

		// Make sure it's not optimizing away the loop above
		{
			float Value = 0;
			for (int32 Index = 0; Index < MAX_VOXELNODE_PINS; Index++)
			{
				Value += Inputs[Index].Get<v_flt>();
				Value += Outputs[Index].Get<v_flt>();
			}
			if (Value == PI)
			{
				UE_LOG(LogVoxel, Log, TEXT("Win!"));
			}
		}

		return End - Start;
	}

	template<typename T>
	void ComputeStatsImpl(const T* This)
	{
		uint64 Duration;
		int32 NumberOfLoops = 1000;
		do
		{
			NumberOfLoops *= 10;
			Duration = ComputeStatsImplEx(This, NumberOfLoops);
		} while (FPlatformTime::ToSeconds64(Duration) < 10e-3); // We want 10ms per node

		FVoxelGraphPerfCounter::Get().SetNodeStats(This, Duration / double(NumberOfLoops));
	}

	VOXELGRAPH_API void ReplaceInputsOutputs(FString& S, const TArray<FString>& Inputs, const TArray<FString>& Outputs);
	VOXELGRAPH_API FString GetPrefixOpLoopString(const TArray<FString>& Inputs, const TArray<FString>& Outputs, int32 InputCount, const FString& Op);
	VOXELGRAPH_API FString GetInfixOpLoopString(const TArray<FString>& Inputs, const TArray<FString>& Outputs, int32 InputCount, const FString& Op);
}
