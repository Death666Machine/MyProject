// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelAxisDependencies.h"
#include "VoxelGraphGeneratorHelpers.h"
#include "Runtime/VoxelNodeType.h"
#include "Runtime/VoxelGraphVMUtils.h"
#include "Runtime/VoxelCompiledGraphs.h"

class UVoxelGraphGenerator;

class VOXELGRAPH_API FVoxelGraphGeneratorInstance : public TVoxelGraphGeneratorInstanceHelper<FVoxelGraphGeneratorInstance, UVoxelGraphGenerator>
{
public:
	class FTarget
	{
	public:
		const FVoxelGraph& Graph;
		mutable FVoxelGraphVMComputeBuffers Buffers;

		struct FBufferX {};
		struct FBufferXY {};
		struct FOutput
		{
			FVoxelNodeType* const GraphOutputs;

			template<typename T, uint32 Index>
			auto& GetRef() { return GraphOutputs[Index].Get<T>(); }
		};

		inline FBufferX GetBufferX() const { return {}; }
		inline FBufferXY GetBufferXY() const { return {}; }
		inline FOutput GetOutputs() const { return { Buffers.GraphOutputs }; }

		inline void ComputeX(const FVoxelContext& Context, FBufferX& BufferX) const
		{
			Compute(EVoxelFunctionAxisDependencies::X, Context);
		}
		inline void ComputeXYWithCache(const FVoxelContext& Context, const FBufferX& BufferX, FBufferXY& BufferXY) const
		{
			Compute(EVoxelFunctionAxisDependencies::XYWithCache, Context);
		}
		inline void ComputeXYZWithCache(const FVoxelContext& Context, const FBufferX& BufferX, const FBufferXY& BufferXY, FOutput& Outputs) const
		{
			Compute(EVoxelFunctionAxisDependencies::XYZWithCache, Context);
		}
		inline void ComputeXYZWithoutCache(const FVoxelContext& Context, FOutput& Outputs) const
		{
			Compute(EVoxelFunctionAxisDependencies::XYZWithoutCache, Context);
		}

	private:
		VOXELGRAPH_API void Compute(EVoxelFunctionAxisDependencies Dependencies, const FVoxelContext& Context) const;
	};

	class FRangeTarget
	{
	public:
		const FVoxelGraph& Graph;
		mutable FVoxelGraphVMComputeRangeBuffers Buffers;

		struct FBufferX {};
		struct FBufferXY {};
		struct FOutput
		{
			FVoxelNodeRangeType* const GraphOutputs;

			template<typename T, uint32 Index>
			auto& GetRef() { return GraphOutputs[Index].Get<T>(); }
		};

		inline FBufferX GetBufferX() const { return {}; }
		inline FBufferXY GetBufferXY() const { return {}; }
		inline FOutput GetOutputs() const { return { Buffers.GraphOutputs }; }

		VOXELGRAPH_API void ComputeXYZWithoutCache(const FVoxelContextRange& Context, FOutput& Outputs) const;
	};

public:
	FVoxelGraphGeneratorInstance(
		const TVoxelSharedRef<FVoxelCompiledGraphs>& Graphs,
		UVoxelGraphGenerator& Generator,
		const TMap<FName, uint32>& FloatOutputs,
		const TMap<FName, uint32>& Int32Outputs);
	~FVoxelGraphGeneratorInstance();

	//~ Begin FVoxelWorldGeneratorInstance Interface
	void Init(const FVoxelWorldGeneratorInit& InitStruct) override final;
	//~ End FVoxelWorldGeneratorInstance Interface

	//~ Begin TVoxelGraphGeneratorInstanceHelper Interface
	template<uint32... InPermutation>
	inline auto GetTarget() const
	{
		static_assert(FVoxelGraphPermutation::IsSorted<InPermutation...>(), "");
		static_assert(sizeof...(InPermutation) == 1 ||
			!FVoxelGraphPermutation::Contains<InPermutation...>(FVoxelGraphOutputsIndices::RangeAnalysisIndex), "");

		auto& Graph = Graphs->GetFast(FVoxelGraphPermutation::Hash<InPermutation...>());
		return FTarget{ *Graph, GetVariablesBuffer(Graph) };
	}
	template<uint32... InPermutation>
	inline auto GetRangeTarget() const
	{
		static_assert(FVoxelGraphPermutation::IsSorted<InPermutation...>(), "");
		static_assert(FVoxelGraphPermutation::Contains<InPermutation...>(FVoxelGraphOutputsIndices::RangeAnalysisIndex), "");

		auto& Graph = Graphs->GetFast(FVoxelGraphPermutation::Hash<InPermutation...>());
		return FRangeTarget{ *Graph, GetRangeVariablesBuffer(Graph) };
	}
	void ReportRangeAnalysisFailure() const;
	//~ End TVoxelGraphGeneratorInstanceHelper Interface

	inline UVoxelGraphGenerator* GetOwner() const
	{
		return Generator.Get();
	}

private:
	const TWeakObjectPtr<UVoxelGraphGenerator> Generator;
	const TVoxelSharedRef<FVoxelCompiledGraphs> Graphs;
	mutable FThreadSafeCounter RangeAnalysisErrors;

	TMap<TVoxelWeakPtr<const FVoxelGraph>, TArray<FVoxelNodeType>> Variables;
	TMap<TVoxelWeakPtr<const FVoxelGraph>, TArray<FVoxelNodeRangeType>> RangeVariables;

	struct FThreadVariables
		: TThreadSingleton<FThreadVariables>
		, TMap<TVoxelWeakPtr<const FVoxelGraph>, TArray<FVoxelNodeType>>
	{
	};
	struct FThreadRangeVariables
		: TThreadSingleton<FThreadRangeVariables>
		, TMap<TVoxelWeakPtr<const FVoxelGraph>, TArray<FVoxelNodeRangeType>>
	{
	};

	FVoxelNodeType* GetVariablesBuffer(const TVoxelWeakPtr<const FVoxelGraph>& Graph) const;
	FVoxelNodeRangeType* GetRangeVariablesBuffer(const TVoxelWeakPtr<const FVoxelGraph>& Graph) const;
};
