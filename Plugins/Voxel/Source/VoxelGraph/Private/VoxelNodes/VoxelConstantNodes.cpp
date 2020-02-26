// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelConstantNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelContext.h"
#include "VoxelNodeFunctions.h"
#include "VoxelGraphGenerator.h"
#include "VoxelWorldGeneratorInit.h"

UVoxelNode_LOD::UVoxelNode_LOD()
{
	SetOutputs(EC::Int);
}
GENERATED_VOXELNODE_IMPL
(
	UVoxelNode_LOD,
	NO_INPUTS,
	DEFINE_OUTPUTS(int32),
	_O0 = _C0.LOD;
)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_VoxelSize::UVoxelNode_VoxelSize()
{
	SetOutputs(EC::Float);
}
TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_VoxelSize::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_VoxelSize& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, VoxelSizeVariable("float", UniqueName.ToString() + "_VoxelSize")
		{
		}

		void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override
		{
			PrivateVariables.Add(VoxelSizeVariable);
		}

		void Init(Seed Inputs[], const FVoxelWorldGeneratorInit& InitStruct) override
		{
			VoxelSize = InitStruct.VoxelSize;
		}
		void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(VoxelSizeVariable.Name + " = " + FVoxelCppIds::InitStruct + ".VoxelSize;");
		}

		void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = VoxelSize;
		}
		void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = VoxelSize;
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *VoxelSizeVariable.Name);
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}

	private:
		float VoxelSize = 0;
		FVoxelVariable const VoxelSizeVariable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_WorldSize::UVoxelNode_WorldSize()
{
	SetOutputs(EC::Int);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_WorldSize::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

			FLocalVoxelComputeNode(const UVoxelNode_WorldSize& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, WorldSizeVariable("uint32", UniqueName.ToString() + "_WorldSize")
		{
		}

		void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override
		{
			PrivateVariables.Add(WorldSizeVariable);
		}

		void Init(Seed Inputs[], const FVoxelWorldGeneratorInit& InitStruct) override
		{
			WorldSize = InitStruct.WorldSize;
		}
		void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(WorldSizeVariable.Name + " = " + FVoxelCppIds::InitStruct + ".WorldSize;");
		}

		void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<int32>() = WorldSize;
		}
		void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<int32>() = WorldSize;
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *WorldSizeVariable.Name);
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}

	private:
		uint32 WorldSize = 0;
		FVoxelVariable const WorldSizeVariable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_CompileTimeConstant::UVoxelNode_CompileTimeConstant()
{
	SetOutputs(EC::Boolean);
}

FText UVoxelNode_CompileTimeConstant::GetTitle() const
{
	return FText::FromName(Name);
}

EVoxelPinCategory UVoxelNode_CompileTimeConstant::GetOutputPinCategory(int32 PinIndex) const
{
	return Type;
}

TSharedPtr<FVoxelCompilationNode> UVoxelNode_CompileTimeConstant::GetCompilationNode() const
{
	auto Result = MakeShared<FVoxelCompileTimeConstantCompilationNode>(*this);
	Result->Name = Name;
	return Result;
}

#if WITH_EDITOR
void UVoxelNode_CompileTimeConstant::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (Graph && GraphNode && PropertyChangedEvent.Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		GraphNode->ReconstructNode();
		Graph->CompileVoxelNodesFromGraphNodes();
	}
}
#endif