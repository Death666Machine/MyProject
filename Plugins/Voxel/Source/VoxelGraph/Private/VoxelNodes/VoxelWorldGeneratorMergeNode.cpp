// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelWorldGeneratorMergeNode.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelCppUtils.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelWorldGeneratorInstance.h"
#include "VoxelGraphOutputsConfig.h"
#include "VoxelNodeFunctions.h"

constexpr int32 NumDefaultInputPins_WGMN = 3 + 2 * 4;

inline TArray<FName> GetFloatOutputs(UVoxelGraphOutputsConfig* Config)
{
	TArray<FName> Result;
	if (Config)
	{
		for (auto& Output : Config->Outputs)
		{
			if (Output.Category == EVoxelDataPinCategory::Float)
			{
				Result.Add(Output.Name);
			}
		}
	}
	return Result;
}

inline TArray<bool> GetComputeFloatOutputs(const TArray<FName>& FloatOutputs, const FVoxelComputeNode& Node)
{
	check(Node.OutputCount == 2 + FloatOutputs.Num() + 1);

	TArray<bool> Result;
	for (int32 Index = 0; Index < FloatOutputs.Num(); Index++)
	{
		Result.Add(Node.IsOutputUsed(2 + Index));
	}
	return Result;
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_WorldGeneratorMerge::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

			FLocalVoxelComputeNode(const UVoxelNode_WorldGeneratorMerge& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Tolerance(Node.Tolerance)
			, MaterialConfig(Node.MaterialConfig)
			, Variable(MakeShared<FVoxelWorldGeneratorArrayVariable>(Node, Node.WorldGenerators))
			, Instances(FVoxelNodeFunctions::CreateWorldGeneratorArray(Node.WorldGenerators))
			, FloatOutputs(GetFloatOutputs(Node.Outputs))
			, CustomData(Node.CustomData)
			, Seeds(Node.Seeds)
			, bComputeValue(IsOutputUsed(0))
			, bComputeMaterial(IsOutputUsed(1))
			, ComputeFloatOutputs(GetComputeFloatOutputs(FloatOutputs, *this))
		{
		}

		virtual void Init(Seed Inputs[], const FVoxelWorldGeneratorInit& InitStruct) override
		{
			auto LocalInitStruct = InitStruct;
			for (int32 I = 0; I < Seeds.Num(); I++)
			{
				LocalInitStruct.Seeds.Add(Seeds[I], Inputs[NumDefaultInputPins_WGMN + CustomData.Num() + I]);
			}
			for (auto& Instance : Instances)
			{
				Instance->Init(LocalInitStruct);
			}
		}
		virtual void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.StartBlock();
			Constructor.AddLinef(TEXT("auto LocalInitStruct = %s;"), *FVoxelCppIds::InitStruct);
			FVoxelCppUtils::CreateMapString(Constructor, "LocalInitStruct.Seeds", Seeds, Inputs, NumDefaultInputPins_WGMN + CustomData.Num());

			Constructor.AddLinef(TEXT("for (auto& Instance : %s)"), *Variable->Name);
			Constructor.StartBlock();
			Constructor.AddLine("Instance->Init(LocalInitStruct);");
			Constructor.EndBlock();
			Constructor.EndBlock();
		}

		virtual void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType InOutputs[], const FVoxelContext& Context) const override
		{
			FVoxelGraphCustomData CustomDataMap;
			for (int32 Index = 0; Index < CustomData.Num(); Index++)
			{
				CustomDataMap.Add(CustomData[Index], Inputs[NumDefaultInputPins_WGMN + Index].Get<v_flt>());
			}

			TArray<v_flt, TInlineAllocator<128>> OutFloatOutputs;
			FVoxelNodeFunctions::ComputeWorldGeneratorsMerge(
				MaterialConfig,
				Tolerance,
				Instances,
				CustomDataMap,
				FloatOutputs,
				Context,
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Inputs[3].Get<int32>(), Inputs[4].Get<v_flt>(),
				Inputs[5].Get<int32>(), Inputs[6].Get<v_flt>(),
				Inputs[7].Get<int32>(), Inputs[8].Get<v_flt>(),
				Inputs[9].Get<int32>(), Inputs[10].Get<v_flt>(),
				bComputeValue, bComputeMaterial, ComputeFloatOutputs,
				InOutputs[0].Get<v_flt>(),
				InOutputs[1].Get<FVoxelMaterial>(),
				OutFloatOutputs,
				InOutputs[OutputCount - 1].Get<int32>());

			for (int32 Index = 0; Index < OutFloatOutputs.Num(); Index++)
			{
				InOutputs[2 + Index].Get<v_flt>() = OutFloatOutputs[Index];
			}
		}
		virtual void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType InOutputs[], const FVoxelContextRange& Context) const override
		{
			FVoxelGraphCustomDataRange CustomDataMap;
			for (int32 Index = 0; Index < CustomData.Num(); Index++)
			{
				CustomDataMap.Add(CustomData[Index], Inputs[NumDefaultInputPins_WGMN + Index].Get<v_flt>());
			}

			InOutputs[0].Get<v_flt>() = 0;

			TArray<TVoxelRange<v_flt>, TInlineAllocator<128>> OutFloatOutputs;
			FVoxelNodeFunctions::ComputeWorldGeneratorsMergeRange(
				Instances,
				CustomDataMap,
				FloatOutputs,
				Context,
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				bComputeValue, ComputeFloatOutputs,
				InOutputs[0].Get<v_flt>(),
				OutFloatOutputs,
				InOutputs[OutputCount - 1].Get<int32>());

			for (int32 Index = 0; Index < OutFloatOutputs.Num(); Index++)
			{
				InOutputs[2 + Index].Get<v_flt>() = OutFloatOutputs[Index];
			}
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& InOutputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.StartBlock();
			Constructor.AddLine("FVoxelGraphCustomData CustomDataMap;");
			FVoxelCppUtils::CreateMapString(Constructor, "CustomDataMap", CustomData, Inputs, NumDefaultInputPins_WGMN);

			Constructor.NewLine();
			Constructor.AddLinef(TEXT("static TArray<FName> StaticFloatOutputs = %s;"), *FVoxelCppUtils::ArrayToString(FloatOutputs));
			Constructor.AddLinef(TEXT("static TArray<bool> StaticComputeFloatOutputs = %s;"), *FVoxelCppUtils::ArrayToString(ComputeFloatOutputs));
			Constructor.NewLine();

			const FString MaterialConfigString =
				MaterialConfig == EVoxelMaterialConfig::RGB
				? "EVoxelMaterialConfig::RGB"
				: MaterialConfig == EVoxelMaterialConfig::SingleIndex
				? "EVoxelMaterialConfig::SingleIndex"
				: "EVoxelMaterialConfig::DoubleIndex";

			Constructor.AddLine("TArray<v_flt, TInlineAllocator<128>> OutFloatOutputs;");
			Constructor.AddLine("FVoxelNodeFunctions::ComputeWorldGeneratorsMerge(");
			Constructor.Indent();
			Constructor.AddLinef(TEXT("%s,"), *MaterialConfigString);
			Constructor.AddLinef(TEXT("%f,"), Tolerance);
			Constructor.AddLinef(TEXT("%s,"), *Variable->Name);
			Constructor.AddLinef(TEXT("CustomDataMap,"));
			Constructor.AddLinef(TEXT("StaticFloatOutputs,"));
			Constructor.AddLinef(TEXT("%s,"), *FVoxelCppIds::Context);
			Constructor.AddLinef(TEXT("%s,"), *Inputs[0]);
			Constructor.AddLinef(TEXT("%s,"), *Inputs[1]);
			Constructor.AddLinef(TEXT("%s,"), *Inputs[2]);
			Constructor.AddLinef(TEXT("%s, %s,"), *Inputs[3], *Inputs[4]);
			Constructor.AddLinef(TEXT("%s, %s,"), *Inputs[5], *Inputs[6]);
			Constructor.AddLinef(TEXT("%s, %s,"), *Inputs[7], *Inputs[8]);
			Constructor.AddLinef(TEXT("%s, %s,"), *Inputs[9], *Inputs[10]);
			Constructor.AddLinef(TEXT("%s, %s,"), *LexToString(bComputeValue), *LexToString(bComputeMaterial));
			Constructor.AddLinef(TEXT("StaticComputeFloatOutputs,"));
			Constructor.AddLinef(TEXT("%s,"), *InOutputs[0]);
			Constructor.AddLinef(TEXT("%s,"), *InOutputs[1]);
			Constructor.AddLinef(TEXT("OutFloatOutputs,"));
			Constructor.AddLinef(TEXT("%s);"), *InOutputs[OutputCount - 1]);
			Constructor.Unindent();

			for (int32 Index = 0; Index < FloatOutputs.Num(); Index++)
			{
				if (ComputeFloatOutputs[Index])
				{
					Constructor.AddLinef(TEXT("%s = OutFloatOutputs[%d];"), *InOutputs[2 + Index], Index);
				}
			}
			Constructor.EndBlock();
		}
		virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& InOutputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.StartBlock();
			Constructor.AddLine("FVoxelGraphCustomDataRange CustomDataMap;");
			FVoxelCppUtils::CreateMapString(Constructor, "CustomDataMap", CustomData, Inputs, NumDefaultInputPins_WGMN);

			Constructor.NewLine();
			Constructor.AddLinef(TEXT("static TArray<FName> StaticFloatOutputs = %s;"), *FVoxelCppUtils::ArrayToString(FloatOutputs));
			Constructor.AddLinef(TEXT("static TArray<bool> StaticComputeFloatOutputs = %s;"), *FVoxelCppUtils::ArrayToString(ComputeFloatOutputs));
			Constructor.NewLine();

			Constructor.AddLine("TArray<TVoxelRange<v_flt>, TInlineAllocator<128>> OutFloatOutputs;");
			Constructor.AddLine("FVoxelNodeFunctions::ComputeWorldGeneratorsMergeRange(");
			Constructor.Indent();
			Constructor.AddLinef(TEXT("%s,"), *Variable->Name);
			Constructor.AddLinef(TEXT("CustomDataMap,"));
			Constructor.AddLinef(TEXT("StaticFloatOutputs,"));
			Constructor.AddLinef(TEXT("%s,"), *FVoxelCppIds::Context);
			Constructor.AddLinef(TEXT("%s,"), *Inputs[0]);
			Constructor.AddLinef(TEXT("%s,"), *Inputs[1]);
			Constructor.AddLinef(TEXT("%s,"), *Inputs[2]);
			Constructor.AddLinef(TEXT("%s,"), *LexToString(bComputeValue));
			Constructor.AddLinef(TEXT("StaticComputeFloatOutputs,"));
			Constructor.AddLinef(TEXT("%s,"), *InOutputs[0]);
			Constructor.AddLinef(TEXT("OutFloatOutputs,"));
			Constructor.AddLinef(TEXT("%s);"), *InOutputs[OutputCount - 1]);
			Constructor.Unindent();

			for (int32 Index = 0; Index < FloatOutputs.Num(); Index++)
			{
				if (ComputeFloatOutputs[Index])
				{
					Constructor.AddLinef(TEXT("%s = OutFloatOutputs[%d];"), *InOutputs[2 + Index], Index);
				}
			}
			Constructor.EndBlock();
		}

		virtual void SetupCpp(FVoxelCppConfig& Config) const override
		{
			Config.AddExposedVariable(Variable);
			Config.AddInclude("VoxelWorldGeneratorPicker.h");
		}

	private:
		const float Tolerance;
		const EVoxelMaterialConfig MaterialConfig;
		const TSharedRef<FVoxelWorldGeneratorArrayVariable> Variable;
		const TArray<TVoxelSharedPtr<FVoxelWorldGeneratorInstance>> Instances;
		const TArray<FName> FloatOutputs;
		const TArray<FName> CustomData;
		const TArray<FName> Seeds;

		const bool bComputeValue;
		const bool bComputeMaterial;
		const TArray<bool> ComputeFloatOutputs;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

UVoxelNode_WorldGeneratorMerge::UVoxelNode_WorldGeneratorMerge()
{
	SetInputs({
		{ "X", EC::Float, "X" },
		{ "Y", EC::Float, "Y" },
		{ "Z", EC::Float, "Z" },
		{ "Index 0", EC::Int, "First generator index" },
		{ "Alpha 0", EC::Float, "First generator alpha" },
		{ "Index 1", EC::Int, "Second generator index" },
		{ "Alpha 1", EC::Float, "Second generator alpha" },
		{ "Index 2", EC::Int, "Third generator index" },
		{ "Alpha 2", EC::Float, "Third generator alpha" },
		{ "Index 3", EC::Int, "Fourth generator index" },
		{ "Alpha 3", EC::Float, "Fourth generator alpha" } });
	check(UVoxelNodeHelper::GetMinInputPins() == NumDefaultInputPins_WGMN);
}

FText UVoxelNode_WorldGeneratorMerge::GetTitle() const
{
	return FText::Format(NSLOCTEXT("Voxel", "WorldGeneratorMerge", "World Generator Merge: {0}"), Super::GetTitle());
}

int32 UVoxelNode_WorldGeneratorMerge::GetOutputPinsCount() const
{
	return 2 + GetFloatOutputs(Outputs).Num() + 1;
}

FName UVoxelNode_WorldGeneratorMerge::GetOutputPinName(int32 PinIndex) const
{
	if (PinIndex == 0)
	{
		return "Value";
	}
	if (PinIndex == 1)
	{
		return "Material";
	}
	if (PinIndex == GetOutputPinsCount() - 1)
	{
		return "Num Queried Generators";
	}
	PinIndex -= 2;
	auto FloatOutputs = GetFloatOutputs(Outputs);
	if (FloatOutputs.IsValidIndex(PinIndex))
	{
		return FloatOutputs[PinIndex];
	}
	return "Error";
}

EVoxelPinCategory UVoxelNode_WorldGeneratorMerge::GetOutputPinCategory(int32 PinIndex) const
{
	if (PinIndex == 1)
	{
		return EC::Material;
	}
	else if (PinIndex == GetOutputPinsCount() - 1)
	{
		return EC::Int;
	}
	else
	{
		return EC::Float;
	}
}

void UVoxelNode_WorldGeneratorMerge::LogErrors(FVoxelGraphErrorReporter& ErrorReporter)
{
	Super::LogErrors(ErrorReporter);

	for (auto& WorldGenerator : WorldGenerators)
	{
		if (!WorldGenerator.IsValid())
		{
			ErrorReporter.AddMessageToNode(this, "invalid world generator", EVoxelGraphNodeMessageType::FatalError);
		}
	}
}

#if WITH_EDITOR
bool UVoxelNode_WorldGeneratorMerge::TryImportFromProperty(UProperty* Property, UObject* Object)
{
	if (auto* ArrayProperty = Cast<UArrayProperty>(Property))
	{
		if (ArrayProperty->Inner->GetCPPType(nullptr, 0) == "FVoxelWorldGeneratorPicker")
		{
			FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Object));
			WorldGenerators.SetNum(ArrayHelper.Num());
			for (int32 Index = 0; Index < ArrayHelper.Num(); Index++)
			{
				WorldGenerators[Index] = *reinterpret_cast<FVoxelWorldGeneratorPicker*>(ArrayHelper.GetRawPtr(Index));
			}
			return true;
		}
	}
	return false;
}
#endif