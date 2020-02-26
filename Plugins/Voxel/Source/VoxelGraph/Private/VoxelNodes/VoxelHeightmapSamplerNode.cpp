// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelHeightmapSamplerNode.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "VoxelAssets/VoxelHeightmapAsset.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelNodeFunctions.h"

UVoxelNode_HeightmapSampler::UVoxelNode_HeightmapSampler()
{
	SetInputs(
		{ "X", EC::Float, "X between 0 and heightmap width" },
		{ "Y", EC::Float, "Y between 0 and heightmap height" });
	SetOutputs(
		{ "Height", EC::Float, "Height at position X Y" },
		{ "Material", EC::Material, "Material at position X Y" },
		{ "Min Height", EC::Float, "Min height of the entire heightmap" },
		{ "Max Height", EC::Float, "Max height of the entire heightmap" },
		{ "Size X", EC::Float, "Width of the heightmap. Affected by the asset XY Scale setting, so it may be a float" },
		{ "Size Y", EC::Float, "Height of the heightmap. Affected by the asset XY Scale setting, so it may be a float" });
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_HeightmapSampler::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_HeightmapSampler& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, bFloat(Node.bFloatHeightmap)
			, SamplerType(Node.SamplerType)
			, DataUINT16(bFloat ? nullptr : Node.HeightmapUINT16)
			, DataFloat(bFloat ? Node.HeightmapFloat : nullptr)
			, Variable(bFloat
				? MakeShared<FVoxelHeightmapVariable>(Node, Node.HeightmapFloat)
				: MakeShared<FVoxelHeightmapVariable>(Node, Node.HeightmapUINT16))
		{
		}

		void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			const auto Lambda = [&](auto& InData)
			{
				if (IsOutputUsed(0)) Outputs[0].Get<v_flt>() = InData.GetHeight(Inputs[0].Get<v_flt>(), Inputs[1].Get<v_flt>(), SamplerType);
				if (IsOutputUsed(1)) Outputs[1].Get<FVoxelMaterial>() = InData.GetMaterial(Inputs[0].Get<v_flt>(), Inputs[1].Get<v_flt>(), SamplerType);
				if (IsOutputUsed(2)) Outputs[2].Get<v_flt>() = InData.GetMinHeight();
				if (IsOutputUsed(3)) Outputs[3].Get<v_flt>() = InData.GetMaxHeight();
				if (IsOutputUsed(4)) Outputs[4].Get<v_flt>() = InData.GetWidth();
				if (IsOutputUsed(5)) Outputs[5].Get<v_flt>() = InData.GetHeight();
			};
			if (bFloat)
			{
				Lambda(DataFloat);
			}
			else
			{
				Lambda(DataUINT16);
			}
		}
		void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			const auto Lambda = [&](auto& InData)
			{
				Outputs[0].Get<v_flt>() = { InData.GetMinHeight(), InData.GetMaxHeight() };
				Outputs[2].Get<v_flt>() = InData.GetMinHeight();
				Outputs[3].Get<v_flt>() = InData.GetMaxHeight();
				Outputs[4].Get<v_flt>() = InData.GetWidth();
				Outputs[5].Get<v_flt>() = InData.GetHeight();
			};
			if (bFloat)
			{
				Lambda(DataFloat);
			}
			else
			{
				Lambda(DataUINT16);
			}
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			const FString SampleTypeString = SamplerType == EVoxelSamplerMode::Clamp ? "EVoxelSamplerMode::Clamp" : "EVoxelSamplerMode::Tile";
			if (IsOutputUsed(0)) Constructor.AddLinef(TEXT("%s = %s.GetHeight(%s, %s, %s);"), *Outputs[0], *Variable->Name, *Inputs[0], *Inputs[1], *SampleTypeString);
			if (IsOutputUsed(1)) Constructor.AddLinef(TEXT("%s = %s.GetMaterial(%s, %s, %s);"), *Outputs[1], *Variable->Name, *Inputs[0], *Inputs[1], *SampleTypeString);
			if (IsOutputUsed(2)) Constructor.AddLinef(TEXT("%s = %s.GetMinHeight();"), *Outputs[2], *Variable->Name);
			if (IsOutputUsed(3)) Constructor.AddLinef(TEXT("%s = %s.GetMaxHeight();"), *Outputs[3], *Variable->Name);
			if (IsOutputUsed(4)) Constructor.AddLinef(TEXT("%s = %s.GetWidth();"), *Outputs[4], *Variable->Name);
			if (IsOutputUsed(5)) Constructor.AddLinef(TEXT("%s = %s.GetHeight();"), *Outputs[5], *Variable->Name);
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			if (IsOutputUsed(0)) Constructor.AddLinef(TEXT("%s = { %s.GetMinHeight(), %s.GetMaxHeight() };"), *Outputs[0], *Variable->Name, *Variable->Name);
			if (IsOutputUsed(2)) Constructor.AddLinef(TEXT("%s = %s.GetMinHeight();"), *Outputs[2], *Variable->Name);
			if (IsOutputUsed(3)) Constructor.AddLinef(TEXT("%s = %s.GetMaxHeight();"), *Outputs[3], *Variable->Name);
			if (IsOutputUsed(4)) Constructor.AddLinef(TEXT("%s = %s.GetWidth();"), *Outputs[4], *Variable->Name);
			if (IsOutputUsed(5)) Constructor.AddLinef(TEXT("%s = %s.GetHeight();"), *Outputs[5], *Variable->Name);
		}
		void SetupCpp(FVoxelCppConfig& Config) const override
		{
			Config.AddExposedVariable(Variable);
			Config.AddInclude("VoxelAssets/VoxelHeightmapAsset.h");
		}

	private:
		const bool bFloat;
		const EVoxelSamplerMode SamplerType;
		const TVoxelHeightmapAssetSamplerWrapper<uint16> DataUINT16;
		const TVoxelHeightmapAssetSamplerWrapper<float> DataFloat;
		const TSharedRef<FVoxelHeightmapVariable> Variable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

FText UVoxelNode_HeightmapSampler::GetTitle() const
{
	return FText::Format(NSLOCTEXT("Voxel", "Heightmap", "Heightmap: {0}"), Super::GetTitle());
}

void UVoxelNode_HeightmapSampler::LogErrors(FVoxelGraphErrorReporter& ErrorReporter)
{
	Super::LogErrors(ErrorReporter);
	if ((bFloatHeightmap && !HeightmapFloat) || (!bFloatHeightmap && !HeightmapUINT16))
	{
		ErrorReporter.AddMessageToNode(this, "invalid heightmap", EVoxelGraphNodeMessageType::FatalError);
	}
}

#if WITH_EDITOR
bool UVoxelNode_HeightmapSampler::TryImportFromProperty(UProperty* Property, UObject* Object)
{
	UVoxelHeightmapAsset* HeightmapAsset = nullptr;
	if (TryImportObject(Property, Object, HeightmapAsset))
	{
		if (bFloatHeightmap)
		{
			HeightmapFloat = Cast<UVoxelHeightmapAssetFloat>(HeightmapAsset);
		}
		else
		{
			HeightmapUINT16 = Cast<UVoxelHeightmapAssetUINT16>(HeightmapAsset);
		}
		return true;
	}
	return false;
}
#endif