// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelSeedNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelNodeColors.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelCppUtils.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "VoxelWorldGeneratorInit.h"
#include "VoxelGraphGlobals.h"

UVoxelSeedNode::UVoxelSeedNode()
{
	SetColor(FVoxelNodeColors::SeedNode);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_Seed::UVoxelNode_Seed()
{
	SetOutputs(EC::Seed);
}

FText UVoxelNode_Seed::GetTitle() const
{
	return FText::FromName(Name);
}

bool UVoxelNode_Seed::CanRenameNode() const
{
	return true;
}

FString UVoxelNode_Seed::GetEditableName() const
{
	return Name.IsNone() ? "" : Name.ToString();
}

void UVoxelNode_Seed::SetEditableName(const FString& NewName)
{
	Name = *NewName;
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_Seed::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelSeedComputeNode
	{
	public:
		FLocalVoxelComputeNode(const UVoxelNode_Seed& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelSeedComputeNode(Node, CompilationNode)
			, DefaultValue(Node.DefaultValue)
			, Name(Node.Name)
		{
		}

		void Init(Seed Inputs[], Seed Outputs[], const FVoxelWorldGeneratorInit& InitStruct) override
		{
			Outputs[0] = InitStruct.Seeds.Contains(Name) ? InitStruct.Seeds[Name] : DefaultValue;
		}
		void InitCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.StartBlock();
			FVoxelCppUtils::DeclareStaticName(Constructor, Name);
			Constructor.AddLine(Outputs[0] + " = " + FVoxelCppIds::InitStruct + ".Seeds.Contains(StaticName) ? " +
				FVoxelCppIds::InitStruct + ".Seeds[StaticName] : " + FString::FromInt(DefaultValue) + ";");
			Constructor.EndBlock();
		}

	private:
		const int32 DefaultValue;
		const FName Name;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_AddSeeds::UVoxelNode_AddSeeds()
{
	SetInputs(EC::Seed);
	SetOutputs(EC::Seed);
	SetInputsCount(1, MAX_VOXELNODE_PINS);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_AddSeeds::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelSeedComputeNode
	{
	public:
		using FVoxelSeedComputeNode::FVoxelSeedComputeNode;

		void Init(Seed Inputs[], Seed Outputs[], const FVoxelWorldGeneratorInit& InitStruct) override
		{
			uint32 X = FVoxelUtilities::MurmurHash32(Inputs[0]);
			for (int32 I = 1; I < InputCount; I++)
			{
				X = FVoxelUtilities::MurmurHash32(X ^ FVoxelUtilities::MurmurHash32(Inputs[I]));
			}
			Outputs[0] = X;
		}
		void InitCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			FString Line;
			Line += Outputs[0] + " = ";

			for (int32 I = 0; I < InputCount - 1; I++)
			{
				Line += "FVoxelUtilities::MurmurHash32(FVoxelUtilities::MurmurHash32(" + Inputs[I] + ") ^ ";
			}
			Line += "FVoxelUtilities::MurmurHash32(" + Inputs[InputCount - 1];

			for (int32 I = 0; I < InputCount; I++)
			{
				Line += ")";
			}
			Line += ";";

			Constructor.AddLine(Line);
		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}