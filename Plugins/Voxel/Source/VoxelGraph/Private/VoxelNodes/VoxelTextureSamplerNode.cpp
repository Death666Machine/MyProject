// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelTextureSamplerNode.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelNodeFunctions.h"
#include "Engine/Texture2D.h"

UVoxelNode_TextureSampler::UVoxelNode_TextureSampler()
{
	SetInputs(
		{ "X", EC::Float, "Coordinate between 0 and texture width" },
		{ "Y", EC::Float, "Coordinate between 0 and texture height" });
	SetOutputs(
		{ "R", EC::Float, "Red between 0 and 1" },
		{ "G", EC::Float, "Green between 0 and 1" },
		{ "B", EC::Float, "Blue between 0 and 1" },
		{ "A", EC::Float, "Alpha between 0 and 1" });
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_TextureSampler::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_TextureSampler& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, bBilinearInterpolation(Node.bBilinearInterpolation)
			, Mode(Node.Mode)
			, Texture(FVoxelTextureUtilities::CreateFromTexture_Color(Node.Texture))
			, Variable(MakeShared<FVoxelColorTextureVariable>(Node, Node.Texture))
		{
		}

		void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			if (bBilinearInterpolation)
			{
				FVoxelNodeFunctions::ReadColorTextureDataFloat(
					Texture,
					Mode,
					Inputs[0].Get<v_flt>(),
					Inputs[1].Get<v_flt>(),
					Outputs[0].Get<v_flt>(),
					Outputs[1].Get<v_flt>(),
					Outputs[2].Get<v_flt>(),
					Outputs[3].Get<v_flt>());
			}
			else
			{
				FVoxelNodeFunctions::ReadColorTextureDataInt(
					Texture,
					Mode,
					Inputs[0].Get<int32>(),
					Inputs[1].Get<int32>(),
					Outputs[0].Get<v_flt>(),
					Outputs[1].Get<v_flt>(),
					Outputs[2].Get<v_flt>(),
					Outputs[3].Get<v_flt>());
			}
		}
		void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			if (bBilinearInterpolation)
			{
				FVoxelNodeFunctions::ReadColorTextureDataFloat(
					Texture,
					Mode,
					Inputs[0].Get<v_flt>(),
					Inputs[1].Get<v_flt>(),
					Outputs[0].Get<v_flt>(),
					Outputs[1].Get<v_flt>(),
					Outputs[2].Get<v_flt>(),
					Outputs[3].Get<v_flt>());
			}
			else
			{
				FVoxelNodeFunctions::ReadColorTextureDataInt(
					Texture,
					Mode,
					Inputs[0].Get<int32>(),
					Inputs[1].Get<int32>(),
					Outputs[0].Get<v_flt>(),
					Outputs[1].Get<v_flt>(),
					Outputs[2].Get<v_flt>(),
					Outputs[3].Get<v_flt>());
			}
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(
				"FVoxelNodeFunctions::" + FString(bBilinearInterpolation ? "ReadColorTextureDataFloat" : "ReadColorTextureDataInt") + "(" +
				Variable->Name + "," +
				(Mode == EVoxelSamplerMode::Clamp ? "EVoxelSamplerMode::Clamp" : "EVoxelSamplerMode::Tile") + "," +
				Inputs[0] + "," +
				Inputs[1] + "," +
				Outputs[0] + "," +
				Outputs[1] + "," +
				Outputs[2] + "," +
				Outputs[3] + ");");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}
		void SetupCpp(FVoxelCppConfig& Config) const override
		{
			Config.AddExposedVariable(Variable);
			Config.AddInclude("Engine/Texture2D.h");
		}

	private:
		const bool bBilinearInterpolation;
		const EVoxelSamplerMode Mode;
		const TVoxelTexture<FColor> Texture;
		const TSharedRef<FVoxelColorTextureVariable> Variable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

EVoxelPinCategory UVoxelNode_TextureSampler::GetInputPinCategory(int32 PinIndex) const
{
	return bBilinearInterpolation ? EC::Float : EC::Int;
}

FText UVoxelNode_TextureSampler::GetTitle() const
{
	return FText::Format(NSLOCTEXT("Voxel", "Texture", "Texture: {0}"), Super::GetTitle());
}

void UVoxelNode_TextureSampler::LogErrors(FVoxelGraphErrorReporter& ErrorReporter)
{
	Super::LogErrors(ErrorReporter);

	FString Error;
	if (!FVoxelTextureUtilities::CanCreateFromTexture(Texture, Error))
	{
		ErrorReporter.AddMessageToNode(this, Error, EVoxelGraphNodeMessageType::FatalError);
	}
}

#if WITH_EDITOR
bool UVoxelNode_TextureSampler::TryImportFromProperty(UProperty* Property, UObject* Object)
{
	return TryImportObject(Property, Object, Texture);
}

void UVoxelNode_TextureSampler::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_STATIC(UVoxelNode_TextureSampler, bBilinearInterpolation) &&
		GraphNode &&
		Graph)
	{
		GraphNode->ReconstructNode();
		Graph->CompileVoxelNodesFromGraphNodes();
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_VoxelTextureSampler::UVoxelNode_VoxelTextureSampler()
{
	SetInputs(
		{ "X", EC::Float, "Coordinate between 0 and texture width" },
		{ "Y", EC::Float, "Coordinate between 0 and texture height" });
	SetOutputs(EC::Float);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_VoxelTextureSampler::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_VoxelTextureSampler& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, bBilinearInterpolation(Node.bBilinearInterpolation)
			, Mode(Node.Mode)
			, Texture(GetGraph()->GetTextureParameter(Node.UniqueName).Texture)
			, Variable(MakeShared<FVoxelFloatTextureVariable>(Node))
		{
		}

		void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			if (bBilinearInterpolation)
			{
				Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::ReadFloatTextureDataFloat(Texture, Mode, Inputs[0].Get<v_flt>(), Inputs[1].Get<v_flt>());
			}
			else
			{
				Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::ReadFloatTextureDataInt(Texture, Mode, Inputs[0].Get<int32>(), Inputs[1].Get<int32>());
			}
		}
		void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			if (bBilinearInterpolation)
			{
				Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::ReadFloatTextureDataFloat(Texture, Mode, Inputs[0].Get<v_flt>(), Inputs[1].Get<v_flt>());
			}
			else
			{
				Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::ReadFloatTextureDataInt(Texture, Mode, Inputs[0].Get<int32>(), Inputs[1].Get<int32>());
			}
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " +
				"FVoxelNodeFunctions::" + FString(bBilinearInterpolation ? "ReadFloatTextureDataFloat" : "ReadFloatTextureDataInt") + "(" +
				Variable->Name + "," +
				(Mode == EVoxelSamplerMode::Clamp ? "EVoxelSamplerMode::Clamp" : "EVoxelSamplerMode::Tile") + "," +
				Inputs[0] + "," +
				Inputs[1] + ");");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}
		void SetupCpp(FVoxelCppConfig& Config) const override
		{
			Config.AddExposedVariable(Variable);
		}

	private:
		const bool bBilinearInterpolation;
		const EVoxelSamplerMode Mode;
		const TVoxelTexture<float> Texture;
		const TSharedRef<FVoxelFloatTextureVariable> Variable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

EVoxelPinCategory UVoxelNode_VoxelTextureSampler::GetInputPinCategory(int32 PinIndex) const
{
	return bBilinearInterpolation ? EC::Float : EC::Int;
}

FText UVoxelNode_VoxelTextureSampler::GetTitle() const
{
	return FText::Format(NSLOCTEXT("Voxel", "Texture", "Voxel Texture: {0}"), Super::GetTitle());
}

#if WITH_EDITOR
bool UVoxelNode_VoxelTextureSampler::TryImportFromProperty(UProperty* Property, UObject* Object)
{
	return false;
}

void UVoxelNode_VoxelTextureSampler::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_STATIC(UVoxelNode_TextureSampler, bBilinearInterpolation) &&
		GraphNode &&
		Graph)
	{
		GraphNode->ReconstructNode();
		Graph->CompileVoxelNodesFromGraphNodes();
	}
}
#endif