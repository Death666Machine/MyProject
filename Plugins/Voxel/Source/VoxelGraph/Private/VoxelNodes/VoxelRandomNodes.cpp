// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelRandomNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "Runtime/VoxelNodeType.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelVariables.h"
#include "VoxelContext.h"
#include "VoxelNodeFunctions.h"

UVoxelNode_RandomFloat::UVoxelNode_RandomFloat()
{
	SetInputs({ "Seed", EC::Seed, "Seed" });
	SetOutputs(EC::Float);
}

FText UVoxelNode_RandomFloat::GetTitle() const
{
	return FText::FromString("Rand Float " + FString::SanitizeFloat(Min) + " " + FString::SanitizeFloat(Max));
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_RandomFloat::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

			FLocalVoxelComputeNode(const UVoxelNode_RandomFloat& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Min(Node.Min)
			, Max(Node.Max)
			, RandFloatVariable("float", UniqueName.ToString() + "_RandFloat")
		{
		}

		void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override
		{
			PrivateVariables.Add(RandFloatVariable);
		}

		void Init(Seed Inputs[], const FVoxelWorldGeneratorInit& InitStruct) override
		{
			RandFloat = FRandomStream(Inputs[0]).FRandRange(Min, Max);
		}
		void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(RandFloatVariable.Name + " = FRandomStream(" + Inputs[0] + ")" + ".FRandRange(" + FString::SanitizeFloat(Min) + "f, " + FString::SanitizeFloat(Max) + "f);");
		}

		void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = RandFloat;
		}
		void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = RandFloat;
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " + RandFloatVariable.Name + ";");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *RandFloatVariable.Name);
		}

	private:
		float Min = 0;
		float Max = 0;
		float RandFloat = 0;
		FVoxelVariable const RandFloatVariable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_RandomInt::UVoxelNode_RandomInt()
{
	SetInputs({ "Seed", EC::Seed, "Seed" });
	SetOutputs(EC::Int);
}

FText UVoxelNode_RandomInt::GetTitle() const
{
	return FText::FromString("Rand Int " + FString::FromInt(Min) + " " + FString::FromInt(Max));
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_RandomInt::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

			FLocalVoxelComputeNode(const UVoxelNode_RandomInt& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Min(Node.Min)
			, Max(Node.Max)
			, RandIntVariable("int32", UniqueName.ToString() + "_RandInt")
		{
		}

		void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override
		{
			PrivateVariables.Add(RandIntVariable);
		}

		void Init(Seed Inputs[], const FVoxelWorldGeneratorInit& InitStruct) override
		{
			RandInt = FRandomStream(Inputs[0]).RandRange(Min, Max);
		}
		void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(RandIntVariable.Name + " = FRandomStream(" + Inputs[0] + ")" + ".RandRange(" + FString::FromInt(Min) + ", " + FString::FromInt(Max) + ");");
		}

		void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<int32>() = RandInt;
		}
		void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<int32>() = RandInt;
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " + RandIntVariable.Name + ";");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *RandIntVariable.Name);
		}

	private:
		int32 Min;
		int32 Max;
		int32 RandInt = 0;
		FVoxelVariable const RandIntVariable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}