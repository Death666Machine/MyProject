// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelOptimizationNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelContext.h"
#include "VoxelNodeFunctions.h"
#include "VoxelMessages.h"
#include "VoxelGraphGenerator.h"

#include "Async/Async.h"

UVoxelNode_StaticClampFloat::UVoxelNode_StaticClampFloat()
{
	SetInputs(EC::Float);
	SetOutputs(EC::Float);
}

FText UVoxelNode_StaticClampFloat::GetTitle() const
{
	return FText::FromString("Static Clamp: " + FString::SanitizeFloat(Min) + " <= X <= " + FString::SanitizeFloat(Max));
}

TSharedPtr<FVoxelCompilationNode> UVoxelNode_StaticClampFloat::GetCompilationNode() const
{
	return MakeShared<FVoxelStaticClampCompilationNode>(*this);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_StaticClampFloat::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

			FLocalVoxelComputeNode(const UVoxelNode_StaticClampFloat& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Min(Node.Min)
			, Max(Node.Max)
		{
		}

		virtual void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = FMath::Clamp<v_flt>(Inputs[0].Get<v_flt>(), Min, Max);
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = FMath::Clamp<v_flt>(%s, %f, %f);"), *Outputs[0], *Inputs[0], Min, Max);
		}
		virtual void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = { Min, Max };
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = { %ff, %ff };"), *Outputs[0], Min, Max);
		}

	private:
		const float Min;
		const float Max;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_RangeAnalysisDebuggerFloat::UVoxelNode_RangeAnalysisDebuggerFloat()
{
	SetInputs(EC::Float);
	SetOutputs(EC::Float);
}

void UVoxelNode_RangeAnalysisDebuggerFloat::UpdateFromBin()
{
	if (Bins.IsValid())
	{
		Min = Bins->bMinMaxInit ? Bins->MinValue : 0;
		Max = Bins->bMinMaxInit ? Bins->MaxValue : 0;
	}
}

void UVoxelNode_RangeAnalysisDebuggerFloat::UpdateGraph()
{
	Curve.GetRichCurve()->Reset();
	Bins->AddToCurve(*Curve.GetRichCurve());
}

void UVoxelNode_RangeAnalysisDebuggerFloat::Reset()
{
	Bins = MakeUnique<FVoxelBins>(GraphMin, GraphMax, GraphStep);
	UpdateFromBin();
}

#if WITH_EDITOR
void UVoxelNode_RangeAnalysisDebuggerFloat::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		Reset();
		UpdateFromBin();
	}
}

void UVoxelNode_RangeAnalysisDebuggerFloat::PostLoad()
{
	Super::PostLoad();
	Reset();
	UpdateFromBin();
}
#endif

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_RangeAnalysisDebuggerFloat::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

			FLocalVoxelComputeNode(const UVoxelNode_RangeAnalysisDebuggerFloat& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Bins(Node.GraphMin, Node.GraphMax, Node.GraphStep)
			, WeakNode(const_cast<UVoxelNode_RangeAnalysisDebuggerFloat*>(&Node))
		{
		}
		~FLocalVoxelComputeNode()
		{
			AsyncTask(ENamedThreads::GameThread, [WeakNode = WeakNode, Bins = Bins]()
			{
				if (WeakNode.IsValid())
				{
					WeakNode->Bins->AddOtherBins(Bins);
					WeakNode->UpdateFromBin();
					WeakNode->UpdateGraph();
				}
				else
				{
					FVoxelMessages::Error(NSLOCTEXT("Voxel", "RangeAnalysisDebugError", "Range Analysis Debugger node was deleted after the graph, values won't be reported"));
				}
			});
		}

		virtual void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = Inputs[0].Get<v_flt>();

			Bins.AddStat(Inputs[0].Get<v_flt>());
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *Inputs[0]);
		}
		virtual void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = Inputs[0].Get<v_flt>();
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *Inputs[0]);
		}

	private:
		mutable FVoxelBins Bins;
		TWeakObjectPtr<UVoxelNode_RangeAnalysisDebuggerFloat> WeakNode;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_Sleep::UVoxelNode_Sleep()
{
	SetInputs(EC::Float);
	SetOutputs(EC::Float);
}

PRAGMA_DISABLE_OPTIMIZATION
TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_Sleep::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

			FLocalVoxelComputeNode(const UVoxelNode_Sleep& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, NumberOfLoops(Node.NumberOfLoops)
		{
		}

		virtual void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			int32 K = 0;
			for (int32 Index = 0; Index < NumberOfLoops; Index++)
			{
				K++;
			}
			check(K == NumberOfLoops);
			Outputs[0] = Inputs[0];
		}

		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *Inputs[0]);
		}
		virtual void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0] = Inputs[0];
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *Inputs[0]);
		}

	private:
		const int32 NumberOfLoops;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}
PRAGMA_ENABLE_OPTIMIZATION

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_RangeUnion::UVoxelNode_RangeUnion()
{
	SetInputs(EC::Float);
	SetOutputs(EC::Float);
	SetInputsCount(2, MAX_VOXELNODE_PINS);
}

GENERATED_VOXELNODE_IMPL_PREFIXOPLOOP(UVoxelNode_RangeUnion, FVoxelNodeFunctions::Union, v_flt)